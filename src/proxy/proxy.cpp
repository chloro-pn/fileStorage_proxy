#include "proxy/proxy.h"
#include "common/message.h"
#include "common/configure.h"
#include "proxy/client_context.h"
#include "proxy/storage_context.h"
#include "json.hpp"
#include <cstdlib>
#include <memory>
#include <vector>

using nlohmann::json;

Proxy::FlowType Proxy::get_flow_id() {
  static uint64_t counter = 0;
  return counter++;
}

Proxy::Proxy(asio::io_context& io, uint16_t p1_port, uint16_t p2_port, std::shared_ptr<spdlog::logger> logger):
             p1_server_(io, p1_port),
             p2_server_(io, p2_port),
             logger_(logger) {
  if(idStorage().init() == false) {
    logger_->critical("hiredis init error.");
    spdlog::shutdown();
    exit(-1);
  }
  p1_server_.setOnConnection([this](std::shared_ptr<TcpConnection> con)->void {
    this->clientOnConnection(con);
  });

  p1_server_.setOnMessage([this](std::shared_ptr<TcpConnection> con)->void {
    this->clientOnMessage(con);
  });

  p1_server_.setOnClose([this](std::shared_ptr<TcpConnection> con)->void {
    this->clientOnClose(con);
  });

  p2_server_.setOnConnection([this](std::shared_ptr<TcpConnection> con)->void {
    this->storageOnConnection(con);
  });

  p2_server_.setOnMessage([this](std::shared_ptr<TcpConnection> con)->void {
    this->storageOnMessage(con);
  });

  p2_server_.setOnClose([this](std::shared_ptr<TcpConnection> con)->void {
    this->storageOnClose(con);
  });
  logger_->trace("proxy server start.");
}

std::vector<std::shared_ptr<TcpConnection>> Proxy::selectSuitableStorageServers() const {
  logger_->trace("func : selectSuitableStorageServers.");
  size_t need_select_ss = Configure::instance().get<int>("block_backup_count");
  if(storage_servers_.size() < need_select_ss) {
    logger_->critical("storage server's count < {}", need_select_ss);
    spdlog::shutdown();
    exit(-1);
  }
  std::vector<std::shared_ptr<TcpConnection>> result;
  for(size_t i = 0; i < need_select_ss; ++i) {
    while(true) {
      auto selected = storage_servers_.begin();
      std::advance(selected, rand() % storage_servers_.size());
      if(std::find(result.begin(), result.end(), *selected) == result.end()) {
        result.push_back(*selected);
        logger_->debug("sserver {} selected.", (*selected)->iport());
        break;
      }
    }
  }
  return result;
}

std::vector<Md5Info> Proxy::getNeedUploadMd5s(const std::vector<Md5Info> &md5s) {
  logger_->trace("func : getNeedUploadMd5s.");
  std::vector<int> flags;
  flags.resize(md5s.size(), 0);
  size_t index = 0;
  for(auto it = md5s.begin(); it!=md5s.end(); ++it) {
    for(auto& each_storage : storage_servers_) {
      StorageContext* storage = each_storage->get_context<StorageContext>();
      if(storage->find(*it)) {
        ++flags[index];
      }
    }
    ++index;
  }

  index = 0;
  std::vector<Md5Info> need_upload_blocks;
  for(auto& each_flag : flags) {
    if(each_flag == 0) {
      need_upload_blocks.push_back(md5s[index]);
    }
    ++index;
  }
  logger_->debug("need upload {} blocks", need_upload_blocks.size());
  return need_upload_blocks;
}

void Proxy::handleClientUploadRequestMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  logger_->trace("handle upload request message");
  ClientContext* client = con->get_context<ClientContext>();

  std::vector<Md5Info> md5s = Message::getMd5FromUploadRequestMessage(j);
  client->setUploadingBlockMd5s(md5s);
  std::vector<Md5Info> need_upload_blocks = getNeedUploadMd5s(md5s);
  std::string message = Message::createUploadResponseMessage(need_upload_blocks);
  con->send(std::move(message));
  client->setState(ClientContext::state::waiting_upload_block);
  logger_->debug("change state to waiting_upload_block");
}

void Proxy::handleClientUploadBlockMessage(std::shared_ptr<TcpConnection> con, json& j) {
  logger_->trace("handle upload block message.");
  ClientContext* client = con->get_context<ClientContext>();
  //distinguish different clients.
  Message::setFlowIdToUploadBlockMessage(j, client->getFlowId());
  if(Message::theFirstBlockPiece(j)) {
    logger_->trace("block {}'s first piece.", Message::getMd5FromUploadBlockMessage(j).getMd5Value());
    client->succFailZero();
    std::weak_ptr<TcpConnection> weak(con);
    auto cb = [this, weak](const Md5Info& md5, bool succ)->void {
      auto con = weak.lock();
      if(!con) {
        logger_->warn("client {} exit before block event handle.{} : {}", con->iport(), md5.getMd5Value(), succ);
        return;
      }
      ClientContext* client = con->get_context<ClientContext>();
      if(succ == false) {
        logger_->warn("vote for fail. {}", md5.getMd5Value());
        client->addFailStorages();
      }
      else {
        logger_->trace("vote for succ. {}", md5.getMd5Value());
        client->addSuccStorages();
      }
      if(client->readyToReplyClient() == true) {
        logger_->trace("all storage server conn call back.");
        if(client->getSuccStorages() >= 1) {
          logger_->trace("block event succ {}", md5.getMd5Value());
          std::string msg = Message::constructUploadBlockAckMessage(md5);
          con->send(msg);
        }
        else {
          logger_->trace("block event fail {}", md5.getMd5Value());
          std::string msg = Message::constructUploadBlockFailMessage(md5);
          con->send(msg);
        }
      }
    };
    Md5Info info = Message::getMd5FromUploadBlockMessage(j);
    auto storage_servers = selectSuitableStorageServers();
    client->transferingSetzero();
    for(auto& each_server : storage_servers) {
      StorageContext* storage = each_server->get_context<StorageContext>();
      client->pushTransferingStorageServers(each_server);
      each_server->send(j.dump());
      logger_->debug("sub block event. {}", info.getMd5Value());
      storage->subBlockAckEvent(info, cb);
    }
  }
  else {
    std::string message = j.dump();
    for(auto& each_trans : client->getTransferingStorageServers()) {
      auto server = each_trans.lock();
      if(server) {
        logger_->trace("relay upload block message {} to {}.", Message::getMd5FromUploadBlockMessage(j).getMd5Value(), server->iport());
        server->send(message);
      }
      else {
          logger_->warn("storage server {} exit on uploading {}.", server->iport(), Message::getMd5FromUploadBlockMessage(j).getMd5Value());
      }
    }
  }
}

void Proxy::handleClientUploadAllBlocksMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  logger_->trace("handle upload all blocks message.");
  ClientContext* client = con->get_context<ClientContext>();
  Md5Info file_id = getFileId(client);
  if(idStorage().findId(file_id) == false) {
    idStorage().storageIdMd5s(file_id, client->getUploadingBlockMd5s());
    logger_->debug("file id {} storage in disk.", file_id.getMd5Value());
  }
  else {
    logger_->warn("file id {} has been stored in disk.", file_id.getMd5Value());
  }
  std::string msg = Message::constructFileStoreSuccMessage(file_id);
  con->send(msg);
  client->setState(ClientContext::state::have_uploaded_all_blocks);
  logger_->trace("change state to have_uploaded_all_blocks.");
}

void Proxy::handleClientDownloadRequestMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  Md5Info file_id = Message::getFileIdFromDownLoadRequestMessage(j);
  bool exist = idStorage().findId(file_id);
  if(exist == false) {
    logger_->debug("client {} : file {} not exist. ",con->iport(), file_id.getMd5Value());
    std::string message = Message::constructFileNotExistMessage(file_id);
    con->send(message);
    //don't change state.
  }
  else {
    ClientContext* context = con->get_context<ClientContext>();
    context->setState(ClientContext::state::waiting_transfer_block);
    logger_->debug("client {} : state change  to waiting transfer block. ", con->iport());
    auto tmp = idStorage().getMd5sFromId(file_id);
    context->setTransferingBlockMd5s(tmp);
    Md5Info request_block = context->getTransferingBlockMd5s()[context->getCurrentTransferBlockIndex()];
    requestBlockFromStorageServer(request_block, con);
    context->moveToNextTransferBlockIndex();
  }
}

void Proxy::requestBlockFromStorageServer(const Md5Info &md5, std::shared_ptr<TcpConnection> client) {

}

void Proxy::clientOnConnection(std::shared_ptr<TcpConnection> con) {
  logger_->warn("on connection, {}", con->iport());
  FlowType id = get_flow_id();
  con->set_context(std::make_shared<ClientContext>(id, logger_));
  clients_[id] = con;
}

void Proxy::clientOnMessage(std::shared_ptr<TcpConnection> con) {
  json message = json::parse(std::string(con->message_data(), con->message_length()));
  std::string message_type = Message::getType(message);
  ClientContext* client = con->get_context<ClientContext>();
  ClientContext::state state = client->getState();
  if(state == ClientContext::state::init) {
    logger_->trace("client state : init");
    if(message_type == "upload_request") {
      handleClientUploadRequestMessage(con, message);
      return;
    }
    else if(message_type == "download_request") {
      handleClientDownloadRequestMessage(con, message);
      return;
    }
    else {
      logger_->error("error message type : {}", message_type);
      con->force_close();
      return;
    }
  }
  else if(state == ClientContext::state::waiting_upload_block) {
    logger_->trace("client state : waiting upload block");
    if(message_type == "upload_block" ) {
      handleClientUploadBlockMessage(con, message);
      return;
    }
    else if(message_type == "upload_all_blocks"){
      handleClientUploadAllBlocksMessage(con, message);
      return;
    }
    else {
      logger_->error("error message type : {}", message_type);
      con->force_close();
      return;
    }
  }
  else if(state == ClientContext::state::waiting_transfer_block) {
    logger_->trace("client state : waiting transfer block");
    if(message_type == "transfer_block") {
      // client->send.
      if(Message::theLastBlockPiece(message)) {
        if(client->continueToTransferBlock()) {
          client->moveToNextTransferBlockIndex();
          Md5Info md5 = client->getTransferingBlockMd5s()[client->getCurrentTransferBlockIndex()];
          requestBlockFromStorageServer(md5, con);
        }
        else {
          //change state to wait transfer all blocks message.
        }
      }
    }
    else {
      logger_->error("error message type : {}", message_type);
      con->force_close();
      return;
    }
  }
  else {
    logger_->error("should not look at this.");
  }
}

void Proxy::clientOnClose(std::shared_ptr<TcpConnection> con) {
  if(con->get_state() != TcpConnection::state::readZero) {
      logger_->error("client {} disconnect state : {}",con->iport(), con->get_state_str());
  }
  else {
      logger_->trace("client {} disconnect.", con->get_state_str());
      con->shutdownw();
  }
  ClientContext* context = con->get_context<ClientContext>();
  FlowType id = context->getFlowId();
  clients_.erase(id);
  if(context->getState() == ClientContext::state::have_uploaded_all_blocks) {
    context->setState(ClientContext::state::file_storage_succ);
    logger_->trace("over.");
    return;
  }
  else {
      logger_->error("context error state on close.");
  }
}

void Proxy::storageOnConnection(std::shared_ptr<TcpConnection> con) {
  logger_->trace("storage server {} connect.", con->iport());
  con->set_context(std::make_shared<StorageContext>(logger_));
  StorageContext* storage = con->get_context<StorageContext>();
  storage->setState(StorageContext::state::waiting_block_set);
  storage_servers_.insert(con);
}

void Proxy::storageOnMessage(std::shared_ptr<TcpConnection> con) {
  json message = json::parse(std::string(con->message_data(), con->message_length()));
  std::string message_type = Message::getType(message);
  StorageContext* storage = con->get_context<StorageContext>();
  if(storage->getState() == StorageContext::state::waiting_block_set) {
    if(message_type == "transfer_block_set") {
      logger_->trace("handle transfer block set message., from {}", con->iport());
      std::vector<Md5Info> md5s = Message::getMd5sFromTransferBlockSetMessage(message);
      for(auto& each : md5s) {
        storage->pushStorageMd5(each);
        logger_->info("transfer block md5 {} from server {}.", each.getMd5Value(), con->iport());
      }
      logger_->trace("get md5s {}", md5s.size());
      if(Message::theLastTransferBlockSet(message)) {
        storage->setState(StorageContext::state::working);
        logger_->debug("server {} transfer block set over.", con->iport());
      }
    }
    else {
      logger_->error("error message type, {} {}.", message_type, con->iport());
      con->force_close();
      return;
    }
  }
  else {
    if(message_type == "upload_block_ack") {
      Md5Info md5_ack = Message::getMd5FromUploadBlockAckMessage(message);
      logger_->trace("handle on upload block ack, {} from {}", md5_ack.getMd5Value(), con->iport());
      storage->pubBlockAckEvent(md5_ack, true);
      storage->pushStorageMd5(md5_ack);
    }
    else if(message_type == "upload_block_fail") {
      Md5Info md5_ack = Message::getMd5FromUploadBlockFailMessage(message);
      logger_->warn("handle on upload block fail, {} from {}", md5_ack.getMd5Value(), con->iport());
      storage->pubBlockAckEvent(md5_ack, false);
    }
    else {
      logger_->error("error message type : {} {}", message_type, con->iport());
      con->force_close();
      return;
    }
  }
}

void Proxy::storageOnClose(std::shared_ptr<TcpConnection> con) {
  StorageContext* storage = con->get_context<StorageContext>();
  storage->handleRemainEvent();
  logger_->trace("storage server {} disconnect.", con->iport());
  logger_->warn("state : {}", con->get_state_str());
  storage_servers_.erase(con);
}
