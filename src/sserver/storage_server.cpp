#include "sserver/storage_server.h"
#include "sserver/storage_server_context.h"
#include "json.hpp"
#include "sserver/block_file.h"
#include "common/md5_info.h"
#include "common/message.h"
#include "md5.h"
#include <vector>
#include <iostream>

using nlohmann::json;

void StorageServer::onConnection(std::shared_ptr<TcpConnection> con) {
  con->set_context(std::make_shared<StorageServerContext>());
  StorageServerContext* state = con->get_context<StorageServerContext>();
  if(state->init() == false) {
    std::cerr << "init fail. force close." << std::endl;
    con->force_close();
    return;
  }
  state->setState(StorageServerContext::state::transfering_block_set);
  state->transferingMd5s() = state->pathStorage().getAllItems();
  sendSomeMd5sToProxy(con);
}

void StorageServer::onMessage(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* context = con->get_context<StorageServerContext>();
  json j = json::parse(std::string(con->message_data(), con->message_length()));

  if(Message::getType(j) == "upload_block") {
    Md5Info block_md5 = Message::getMd5FromUploadBlockMessage(j);
    //check if the same md5 block is uploading or have been stored in disk.
    Md5Info md5 = Message::getMd5FromUploadBlockMessage(j);
    context->uploadingMd5s()[md5].append(Message::getContentFromUploadBlockMessage(j));
    if(Message::theLastBlockPiece(j) == true) {
      Md5Info md5 = Message::getMd5FromUploadBlockMessage(j);
      MD5 tmp(context->uploadingMd5s()[md5]);
      std::string m1((const char*)tmp.getDigest(), 16);
      if(m1 != md5.getMd5Value()) {
        context->uploadingMd5s().erase(md5);
        std::string fail_message = Message::constructUploadBlockFailMessage(md5);
        con->send(fail_message);
        context->uploadingMd5s().erase(md5);
      }
      else {
        BlockFile bf;
        bf.createNewFile(context->getBlockFilePath(md5));
        bf.writeBlock(context->uploadingMd5s()[md5]);
        context->pathStorage().storageItemPath(md5, context->getBlockFilePath(md5));
        std::string ack_message = Message::constructUploadBlockAckMessage(md5);
        con->send(ack_message);
        context->uploadingMd5s().erase(md5);
      }
    }
  }
  else {
    std::cerr << "error state." << std::endl;
    con->force_close();
    return;
  }
}

void StorageServer::onWriteComplete(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* context = con->get_context<StorageServerContext>();
  if(context->getState() == StorageServerContext::state::transfering_block_set) {
    sendSomeMd5sToProxy(con);
  }
  else if(context->getState() == StorageServerContext::state::working) {
    //continue sending block.
    //if current block send over.
  }
  else {
    std::cerr << "storage server should be working state." << std::endl;
    con->force_close();
    return;
  }
}

void StorageServer::onClose(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* context = con->get_context<StorageServerContext>();
  for(auto& each : context->uploadingMd5s()) {
    std::string fail_message = Message::constructUploadBlockFailMessage(each.first);
    con->send(fail_message);
  }
  std::cout << "close state : " << con->get_state_str() << std::endl;
}

StorageServer::StorageServer(asio::io_context& io, std::string ip, std::string port):
                             client_(io, ip, port) {
  client_.setOnConnection([this](std::shared_ptr<TcpConnection> con)->void {
    this->onConnection(con);
  });

  client_.setOnMessage([this](std::shared_ptr<TcpConnection> con)->void {
    this->onMessage(con);
  });

  client_.setOnWriteComplete([this](std::shared_ptr<TcpConnection> con)->void {
    this->onWriteComplete(con);
  });

  client_.setOnClose([this](std::shared_ptr<TcpConnection> con)->void {
    this->onClose(con);
  });
}

void StorageServer::connectToProxyServer() {
  client_.connect();
}

void StorageServer::sendSomeMd5sToProxy(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* state = con->get_context<StorageServerContext>();
  std::vector<Md5Info> now_trans;
  for(;state->nextToTransferMd5Index() < 1024; ++(state->nextToTransferMd5Index())) {
    if(state->nextToTransferMd5Index() >= state->transferingMd5s().size()) {
      break;
    }
    now_trans.push_back(state->transferingMd5s()[state->nextToTransferMd5Index()]);
  }
  std::string message;
  if(state->nextToTransferMd5Index() >= state->transferingMd5s().size()) {
    message = Message::constructTransferBlockSetMessage(now_trans, true);
    state->nextToTransferMd5Index() = 0;
    state->transferingMd5s().clear();
    state->setState(StorageServerContext::state::working);
  }
  else {
    message = Message::constructTransferBlockSetMessage(now_trans, false);
  }
  con->send(std::move(message));
}
