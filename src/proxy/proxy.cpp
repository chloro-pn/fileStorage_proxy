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
    SPDLOG_LOGGER_CRITICAL(logger_, "hiredis init error.");
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
  SPDLOG_LOGGER_TRACE(logger_, "proxy server start.");
}
std::vector<std::shared_ptr<TcpConnection>> Proxy::selectSuitableStorageServers() const {
  size_t need_select_ss = Configure::instance().get<int>("block_backup_count");
  if(storage_servers_.size() < need_select_ss) {
    SPDLOG_LOGGER_CRITICAL(logger_, "storage server's count < {}", need_select_ss);
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
        SPDLOG_LOGGER_DEBUG(logger_, "sserver {} selected.", (*selected)->iport());
        break;
      }
    }
  }
  return result;
}

std::vector<Md5Info> Proxy::getNeedUploadMd5s(const std::vector<Md5Info> &md5s) {
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
  SPDLOG_LOGGER_DEBUG(logger_, "need upload {} blocks", need_upload_blocks.size());
  return need_upload_blocks;
}

void Proxy::handleClientUploadRequestMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  ClientContext* client = con->get_context<ClientContext>();
  std::vector<Md5Info> md5s = Message::getMd5FromUploadRequestMessage(j);
  client->setUploadingBlockMd5s(md5s);
  std::vector<Md5Info> need_upload_blocks = getNeedUploadMd5s(md5s);
  std::string message = Message::createUploadResponseMessage(need_upload_blocks);
  con->send(std::move(message));
  client->setState(ClientContext::state::waiting_upload_block);
  SPDLOG_LOGGER_DEBUG(logger_, "change state to waiting_upload_block");
}

void Proxy::handleClientUploadBlockMessage(std::shared_ptr<TcpConnection> con, json& j) {
  ClientContext* client = con->get_context<ClientContext>();
  //distinguish different clients.
  Message::setFlowIdToUploadBlockMessage(j, client->getFlowId());
  if(Message::theFirstBlockPiece(j)) {
    SPDLOG_LOGGER_TRACE(logger_, "block {}'s first piece.", Message::getMd5FromUploadBlockMessage(j).getMd5Value());
    client->succFailZero();
    std::weak_ptr<TcpConnection> weak(con);
    auto cb = [this, weak](const Md5Info& md5, bool succ)->void {
      auto con = weak.lock();
      if(!con) {
        SPDLOG_LOGGER_WARN(logger_, "client {} exit before block event handle.{} : {}", con->iport(), md5.getMd5Value(), succ);
        return;
      }
      ClientContext* client = con->get_context<ClientContext>();
      if(succ == false) {
        SPDLOG_LOGGER_WARN(logger_, "vote for fail. {}", md5.getMd5Value());
        client->addFailStorages();
      }
      else {
        SPDLOG_LOGGER_TRACE(logger_, "vote for succ. {}", md5.getMd5Value());
        client->addSuccStorages();
      }
      if(client->readyToReplyClient() == true) {
        SPDLOG_LOGGER_TRACE(logger_, "all storage server conn call back.");
        if(client->getSuccStorages() >= 1) {
          SPDLOG_LOGGER_TRACE(logger_, "block event succ {}", md5.getMd5Value());
          std::string msg = Message::constructUploadBlockAckMessage(md5);
          con->send(msg);
        }
        else {
          SPDLOG_LOGGER_TRACE(logger_, "block event fail {}", md5.getMd5Value());
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
/*********** careful **************/
      each_server->send(j.dump());
/**********************************/
      SPDLOG_LOGGER_DEBUG(logger_, "sub block event. {}", info.getMd5Value());
      storage->subBlockAckEvent(info, cb);
    }
  }
  else {
    std::string message = j.dump();
    for(auto& each_trans : client->getTransferingStorageServers()) {
      auto server = each_trans.lock();
      if(server) {
        SPDLOG_LOGGER_TRACE(logger_, "relay upload block message {} to {}.", Message::getMd5FromUploadBlockMessage(j).getMd5Value(), server->iport());
        server->send(message);
      }
      else {
        SPDLOG_LOGGER_WARN(logger_, "storage server {} exit on uploading {}.", server->iport(), Message::getMd5FromUploadBlockMessage(j).getMd5Value());
      }
    }
  }
}

void Proxy::handleClientUploadAllBlocksMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  ClientContext* client = con->get_context<ClientContext>();
  Md5Info file_id = getFileId(client);
  if(idStorage().findId(file_id) == false) {
    idStorage().storageIdMd5s(file_id, client->getUploadingBlockMd5s());
    SPDLOG_LOGGER_DEBUG(logger_, "file id {} storage in disk.", file_id.getMd5Value());
  }
  else {
    SPDLOG_LOGGER_WARN(logger_, "file id {} has been stored in disk.", file_id.getMd5Value());
  }
  std::string msg = Message::constructFileStoreSuccMessage(file_id);
  con->send(msg);
  client->setState(ClientContext::state::have_uploaded_all_blocks);
  SPDLOG_LOGGER_TRACE(logger_, "change state to have_uploaded_all_blocks.");
}

void Proxy::handleClientDownloadRequestMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  Md5Info file_id = Message::getFileIdFromDownLoadRequestMessage(j);
  bool exist = idStorage().findId(file_id);
  if(exist == false) {
    SPDLOG_LOGGER_DEBUG(logger_, "client {} : file {} not exist. ",con->iport(), file_id.getMd5Value());
    std::string message = Message::constructFileNotExistMessage(file_id);
    con->send(message);
    //don't change state.
  }
  else {
    ClientContext* context = con->get_context<ClientContext>();
    context->setState(ClientContext::state::waiting_transfer_block);
    SPDLOG_LOGGER_DEBUG(logger_, "client {} : state change  to waiting transfer block. ", con->iport());
    auto tmp = idStorage().getMd5sFromId(file_id);
    std::vector<Md5Info> dont_need = Message::getHaveDownloadMd5sFromDownLoadRequestMessage(j);
    std::vector<Md5Info> really_need;
    for(auto& each : tmp) {
      auto it = std::find_if(dont_need.begin(), dont_need.end(), [each](const Md5Info& md5)->bool {
        return each.getMd5Value() == md5.getMd5Value();
      });
      if(it == dont_need.end()) {
        // really need.
        really_need.push_back(*it);
      }
    }

    context->setTransferingBlockMd5s(really_need);
    Md5Info request_block = context->getTransferingBlockMd5s()[context->getCurrentTransferBlockIndex()];
    requestBlockFromStorageServer(request_block, con);
    context->moveToNextTransferBlockIndex();
  }
}

void Proxy::requestBlockFromStorageServer(const Md5Info &md5, std::shared_ptr<TcpConnection> client) {
  std::vector<std::shared_ptr<TcpConnection>> servers = findServersStoragedBlock(md5);
  if(servers.empty() == true) {
    SPDLOG_LOGGER_ERROR(logger_, "block {} is not exist.", md5.getMd5Value());
    client->force_close();
    return;
  }
  std::shared_ptr<TcpConnection> server = selectStorageServer(servers);
  std::string message = Message::constructDownLoadBlockMessage(md5);
  server->send(std::move(message));
}

std::vector<std::shared_ptr<TcpConnection>> Proxy::findServersStoragedBlock(const Md5Info& md5) {
  std::vector<std::shared_ptr<TcpConnection>> result;
  for(auto& each : storage_servers_) {
    StorageContext* context = each->get_context<StorageContext>();
    if(context->find(md5) == true) {
      result.push_back(each);
    }
  }
  return result;
}

std::shared_ptr<TcpConnection> Proxy::selectStorageServer(const std::vector<std::shared_ptr<TcpConnection>>& ss) {
  //just for current version.
  return ss.front();
}

void Proxy::clientOnConnection(std::shared_ptr<TcpConnection> con) {
  SPDLOG_LOGGER_WARN(logger_, "on connection, {}", con->iport());
  FlowType id = get_flow_id();
  con->set_context(std::make_shared<ClientContext>(id, logger_));
  clients_[id] = con;
  con->get_next_message();
}

void Proxy::clientOnMessage(std::shared_ptr<TcpConnection> con) {
  json message = json::parse(std::string(con->message_data(), con->message_length()));
  std::string message_type = Message::getType(message);
  ClientContext* client = con->get_context<ClientContext>();
  ClientContext::state state = client->getState();
  if(state == ClientContext::state::init) {
    SPDLOG_LOGGER_TRACE(logger_, "client state : init");
    if(message_type == "upload_request") {
      handleClientUploadRequestMessage(con, message);
    }
    else if(message_type == "download_request") {
      handleClientDownloadRequestMessage(con, message);
    }
    else {
      SPDLOG_LOGGER_ERROR(logger_, "error message type : {}", message_type);
      con->force_close();
      return;
    }
  }
  else if(state == ClientContext::state::waiting_upload_block) {
    if(message_type == "upload_block" ) {
      handleClientUploadBlockMessage(con, message);
    }
    else if(message_type == "upload_all_blocks"){
      handleClientUploadAllBlocksMessage(con, message);
    }
    else {
      SPDLOG_LOGGER_ERROR(logger_, "error message type : {}", message_type);
      con->force_close();
      return;
    }
  }
  else if(state == ClientContext::state::waiting_transfer_block) {
    if(message_type == "transfer_block_ack") {
      if(client->continueToTransferBlock()) {
        client->moveToNextTransferBlockIndex();
        Md5Info md5 = client->getTransferingBlockMd5s()[client->getCurrentTransferBlockIndex()];
        requestBlockFromStorageServer(md5, con);
      }
    }
    else if(message_type == "transfer_all_blocks") {
      if(client->continueToTransferBlock() != false) {
        SPDLOG_LOGGER_ERROR(logger_, "client {} has some blocks need to send.", con->iport());
        con->force_close();
        return;
      }
      //change state to wait transfer all blocks message.
      client->setState(ClientContext::state::have_transfered_all_blocks);
      SPDLOG_LOGGER_INFO(logger_, "client : {} change state to have transfered all blocks. ", con->iport());
    }
    else {
      SPDLOG_LOGGER_ERROR(logger_, "error message type : {}", message_type);
      con->force_close();
      return;
    }
  }
  else {
    SPDLOG_LOGGER_ERROR(logger_, "should not look at this.");
    con->force_close();
  }
  con->get_next_message();
}

void Proxy::clientOnClose(std::shared_ptr<TcpConnection> con) {
  if(con->get_state() != TcpConnection::state::readZero) {
    SPDLOG_LOGGER_ERROR(logger_, "client {} disconnect state : {}",con->iport(), con->get_state_str());
  }
  else {
    SPDLOG_LOGGER_TRACE(logger_, "client {} disconnect.", con->get_state_str());
    con->shutdownw();
  }
  ClientContext* context = con->get_context<ClientContext>();
  FlowType id = context->getFlowId();
  clients_.erase(id);

  if(context->getState() == ClientContext::state::have_uploaded_all_blocks) {
    context->setState(ClientContext::state::file_storage_succ);
    SPDLOG_LOGGER_TRACE(logger_, "file storage succ.");
    return;
  }
  else if(context->getState() == ClientContext::state::have_transfered_all_blocks) {
    context->setState(ClientContext::state::file_download_succ);
    SPDLOG_LOGGER_TRACE(logger_, "file download succ");
    return;
  }
  else {
    SPDLOG_LOGGER_ERROR(logger_, "context error state on close.");
  }
}

void Proxy::storageOnConnection(std::shared_ptr<TcpConnection> con) {
  SPDLOG_LOGGER_DEBUG(logger_, "storage server {} connect.", con->iport());
  con->set_context(std::make_shared<StorageContext>(logger_));
  StorageContext* storage = con->get_context<StorageContext>();
  storage->setState(StorageContext::state::waiting_block_set);
  storage_servers_.insert(con);
  con->get_next_message();
}

void Proxy::storageOnMessage(std::shared_ptr<TcpConnection> con) {
  json message = json::parse(std::string(con->message_data(), con->message_length()));
  std::string message_type = Message::getType(message);
  StorageContext* storage = con->get_context<StorageContext>();
  if(storage->getState() == StorageContext::state::waiting_block_set) {
    if(message_type == "transfer_block_set") {
      SPDLOG_LOGGER_TRACE(logger_, "handle transfer block set message., from {}", con->iport());
      std::vector<Md5Info> md5s = Message::getMd5sFromTransferBlockSetMessage(message);
      for(auto& each : md5s) {
        storage->pushStorageMd5(each);
        SPDLOG_LOGGER_INFO(logger_, "transfer block md5 {} from server {}.", each.getMd5Value(), con->iport());
      }
      SPDLOG_LOGGER_TRACE(logger_, "get md5s {}", md5s.size());
      if(Message::theLastTransferBlockSet(message)) {
        storage->setState(StorageContext::state::working);
        SPDLOG_LOGGER_DEBUG(logger_, "server {} transfer block set over.", con->iport());
      }
    }
    else {
      SPDLOG_LOGGER_ERROR(logger_, "error message type, {} {}.", message_type, con->iport());
      con->force_close();
      return;
    }
  }
  else {
    if(message_type == "upload_block_ack") {
      Md5Info md5_ack = Message::getMd5FromUploadBlockAckMessage(message);
      SPDLOG_LOGGER_TRACE(logger_, "handle on upload block ack, {} from {}", md5_ack.getMd5Value(), con->iport());
      storage->pubBlockAckEvent(md5_ack, true);
      storage->pushStorageMd5(md5_ack);
    }
    else if(message_type == "upload_block_fail") {
      Md5Info md5_ack = Message::getMd5FromUploadBlockFailMessage(message);
      SPDLOG_LOGGER_WARN(logger_, "handle on upload block fail, {} from {}", md5_ack.getMd5Value(), con->iport());
      storage->pubBlockAckEvent(md5_ack, false);
    }
    else if(message_type == "transfer_block") {
      FlowType id = Message::getFlowIdFromTransferBlockMessage(message);
      auto client = clients_.find(id);
      if(client == clients_.end()) {
        SPDLOG_LOGGER_ERROR(logger_, "client {} disconnect when transfering block. ", id);
        con->force_close();
        return;
      }
      std::string msg(con->message_data(), con->message_length());
      client->second->send(std::move(msg));
    }
    else {
      SPDLOG_LOGGER_ERROR(logger_, "error message type : {} {}", message_type, con->iport());
      con->force_close();
      return;
    }
  }
  con->get_next_message();
}

void Proxy::storageOnClose(std::shared_ptr<TcpConnection> con) {
  StorageContext* storage = con->get_context<StorageContext>();
  storage->handleRemainEvent();
  SPDLOG_LOGGER_TRACE(logger_, "storage server {} disconnect.", con->iport());
  SPDLOG_LOGGER_WARN(logger_, "state : {}", con->get_state_str());
  storage_servers_.erase(con);
}
