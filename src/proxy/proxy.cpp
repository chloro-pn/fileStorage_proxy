#include "proxy/proxy.h"
#include "common/message.h"
#include "proxy/client_context.h"
#include "proxy/storage_context.h"
#include "json.hpp"
#include <iostream>
#include <memory>
#include <vector>

using nlohmann::json;

Proxy::Proxy(asio::io_context& io, uint16_t p1_port, uint16_t p2_port):
             p1_server_(io, p1_port),
             p2_server_(io, p2_port) {
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
}

std::vector<std::shared_ptr<TcpConnection>> Proxy::selectSuitableStorageServers() const {
  //just for test.
  return { *storage_servers_.begin() };
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
}

void Proxy::handleClientUploadBlockMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  ClientContext* client = con->get_context<ClientContext>();
  if(Message::theFirstBlockPiece(j)) {
    client->succFailZero();
    auto cb = [this, con](const Md5Info& md5, bool succ)->void {
      if(clients_.find(con) == clients_.end()) {
        return;
      }
      ClientContext* client = con->get_context<ClientContext>();
      if(succ == false) {
        client->addFailStorages();
      }
      else {
        client->addSuccStorages();
      }
      if(client->readyToReplyClient() == true) {
        if(client->getSuccStorages() >= 2) {
          std::string msg = Message::constructUploadBlockAckMessage(md5);
          con->send(msg);
        }
        else {
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
      storage->subBlockAckEvent(info, cb);
    }
  }
  else {
    std::string message = j.dump();
    for(auto& each_trans : client->getTransferingStorageServers()) {
      auto server = each_trans.lock();
      if(server) {
        server->send(message);
      }
    }
  }
}

void Proxy::handleClientUploadAllBlocksMessage(std::shared_ptr<TcpConnection> con, const json& j) {
  ClientContext* client = con->get_context<ClientContext>();
  Md5Info file_id = getFileId(client);
  if(idStorage().findId(file_id) == false) {
    idStorage().storageIdMd5s(file_id, client->getUploadingBlockMd5s());
  }
  std::string msg = Message::constructFileStoreSuccMessage(file_id);
  con->send(msg);
  client->setState(ClientContext::state::have_uploaded_all_blocks);
}

void Proxy::clientOnConnection(std::shared_ptr<TcpConnection> con) {
  std::cout << "client : " << con->iport() << "connect. " << std::endl;
  con->set_context(std::make_shared<ClientContext>());
  clients_.insert(con);
  if(idStorage().init() == false) {
    con->force_close();
  }
}

void Proxy::clientOnMessage(std::shared_ptr<TcpConnection> con) {
  json message = json::parse(std::string(con->message_data(), con->message_length()));
  std::string message_type = Message::getType(message);
  ClientContext* client = con->get_context<ClientContext>();
  ClientContext::state state = client->getState();
  if(state == ClientContext::state::init) {
    if(message_type != "upload_request") {
      std::cerr << "error message type: " << message_type << std::endl;
      con->force_close();
      return;
    }
    else {
      handleClientUploadRequestMessage(con, message);
      return;
    }
  }
  else if(state == ClientContext::state::waiting_upload_block) {
    if(message_type == "upload_block" ) {
      handleClientUploadBlockMessage(con, message);
      return;
    }
    else if(message_type == "upload_all_blocks"){
      handleClientUploadAllBlocksMessage(con, message);
      return;
    }
    else {
      std::cerr << "error message: " << message_type << std::endl;
      con->force_close();
      return;
    }
  }
  else {
    std::cerr << "should get nothing." << std::endl;
  }
}

void Proxy::clientOnClose(std::shared_ptr<TcpConnection> con) {
  std::cout << "client : " << con->iport() << "disconnect. ";
  std::cout << "state : " << con->get_state_str() << std::endl;
  clients_.erase(con);
  if(con->get_state() == TcpConnection::state::readZero) {
    ClientContext* client = con->get_context<ClientContext>();
    if(client->getState() == ClientContext::state::have_uploaded_all_blocks) {
      client->setState(ClientContext::state::file_storage_succ);
      std::cerr << "client close normally." << std::endl;
      con->shutdownw();
      return;
    }
  }
}

void Proxy::storageOnConnection(std::shared_ptr<TcpConnection> con) {
  std::cout << "storage server : " << con->iport() << "connect." << std::endl;
  con->set_context(std::make_shared<StorageContext>());
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
      std::vector<Md5Info> md5s = Message::getMd5sFromTransferBlockSetMessage(message);
      for(auto& each : md5s) {
        storage->pushStorageMd5(each);
        std::cout << "get md5 : " << each.getMd5Value() << std::endl;
      }
      if(Message::theLastTransferBlockSet(message)) {
        storage->setState(StorageContext::state::working);
      }
    }
    else {
      std::cerr << "err message type : " << message_type << std::endl;
      con->force_close();
      return;
    }
  }
  else {
    if(message_type == "upload_block_ack") {
      Md5Info md5_ack = Message::getMd5FromUploadBlockAckMessage(message);
      storage->pubBlockAckEvent(md5_ack, true);
      storage->pushStorageMd5(md5_ack);
    }
    else if(message_type == "upload_block_fail") {
      Md5Info md5_ack = Message::getMd5FromUploadBlockFailMessage(message);
      storage->pubBlockAckEvent(md5_ack, false);
    }
    else {
      std::cerr << "error message type : " << message_type << std::endl;
      con->force_close();
      return;
    }
  }
}

void Proxy::storageOnClose(std::shared_ptr<TcpConnection> con) {
  StorageContext* storage = con->get_context<StorageContext>();
  storage->handleRemainEvent();
  std::cout << "storage server : " << con->iport() << "disconnect. ";
  std::cout << "state : " << con->get_state_str() << std::endl;
  storage_servers_.erase(con);
}
