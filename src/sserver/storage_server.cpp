#include "sserver/storage_server.h"
#include "sserver/storage_server_context.h"
#include "json.hpp"
#include "sserver/block_file.h"
#include "common/md5_info.h"
#include "common/message.h"
#include "md5.h"
#include <vector>
#include <cassert>
#include <tuple>

using nlohmann::json;

void StorageServer::onConnection(std::shared_ptr<TcpConnection> con) {
  SPDLOG_LOGGER_DEBUG(logger_, "storage server {} connect.", con->iport());
  con->set_context(std::make_shared<StorageServerContext>());
  StorageServerContext* state = con->get_context<StorageServerContext>();
  state->setState(StorageServerContext::state::transfering_block_set);
  state->transferingMd5s() = pathStorage().getAllItems();
  sendSomeMd5sToProxy(con);
  con->get_next_message();
}

void StorageServer::sendSomeMd5PieceToProxy(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* context = con->get_context<StorageServerContext>();
  auto& download_context = context->downloadingContext();
  if(download_context.empty() == true) {
    SPDLOG_LOGGER_DEBUG(logger_, "send over.");
    return;
  }
  auto need = download_context.begin();
  uint64_t flow_id = need->first;
  Md5Info md5 = std::get<0>(need->second);
  size_t& offset = std::get<1>(need->second);
  uint32_t& index = std::get<2>(need->second);

  const constexpr size_t each_send_size = 4096;
  if(context->downloadingMd5s().find(md5) == context->downloadingMd5s().end()) {
    SPDLOG_LOGGER_CRITICAL(logger_, "need md5 not exist.");
    spdlog::shutdown();
    exit(-1);
  }
  std::string& md5_content = context->downloadingMd5s()[md5].second;
  uint32_t& ref = context->downloadingMd5s()[md5].first;
  assert(ref > 0);

  if(offset + each_send_size >= md5_content.size()) {
    std::string tmp = md5_content.substr(offset, md5_content.size() - offset);
    std::string message = Message::constructTransferBlockMessage(md5, index, true, flow_id, std::move(tmp));
    con->send(std::move(message));
    //clean
    download_context.erase(flow_id);
    //--md5's ref.
    --ref;
    if(ref == 0) {
      context->downloadingMd5s().erase(md5);
    }
    SPDLOG_LOGGER_DEBUG(logger_, "the last piece of md5 : {}", md5.getMd5Value());
  }
  else {
    std::string tmp = md5_content.substr(offset, each_send_size);
    std::string message = Message::constructTransferBlockMessage(md5, index, false, flow_id, std::move(tmp));
    con->send(std::move(message));
    //update context;
    offset += each_send_size;
    index += 1;
  }
}

void StorageServer::onMessage(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* context = con->get_context<StorageServerContext>();
  json j = json::parse(std::string(con->message_data(), con->message_length()));

  if(Message::getType(j) == "upload_block") {
    Md5Info md5 = Message::getMd5FromUploadBlockMessage(j);
    //check if the same md5 block is uploading or have been stored in disk.
    uint64_t flow = Message::getFlowIdFromUploadBlockMessage(j);
    std::string content = Message::getContentFromUploadBlockMessage(j);

    if(Message::theFirstBlockPiece(j) == true) {
      if(context->uploadingFlowIds().find(md5) == context->uploadingFlowIds().end()) {
        context->uploadingFlowIds()[md5] = flow;
      }
    }

    if(context->uploadingFlowIds()[md5] != flow) {
      goto end_point;
    }
    context->uploadingMd5s()[md5].append(content);

    if(Message::theLastBlockPiece(j) == true) {
      std::string md5_value = MD5(context->uploadingMd5s()[md5]).toStr();
      if(md5_value != md5.getMd5Value()) {
        std::string fail_message = Message::constructUploadBlockFailMessage(md5);
        SPDLOG_LOGGER_WARN(logger_, "upload block fail. {} != {}", md5.getMd5Value(), md5_value);
        con->send(fail_message);
      }
      else {
        bool exist = pathStorage().checkItem(md5);
        if(exist == false) {
          pathStorage().storageItemPath(md5, context->getBlockFilePath(md5));
          SPDLOG_LOGGER_DEBUG(logger_, "md5 {} store in path storage.", md5.getMd5Value());
        }
        //
        if(BlockFile::fileExist(context->getBlockFilePath(md5)) == false) {
          BlockFile bf;
          bool succ = bf.createNewFile(context->getBlockFilePath(md5));
          if(succ == false) {
            SPDLOG_LOGGER_ERROR(logger_, "create new file error.");
            spdlog::shutdown();
            exit(-1);
          }
          bf.writeBlock(context->uploadingMd5s()[md5]);
        }
        std::string ack_message = Message::constructUploadBlockAckMessage(md5);
        SPDLOG_LOGGER_DEBUG(logger_, "send upload block ack. {}", md5.getMd5Value());
        con->send(ack_message);
      }
      context->uploadingMd5s().erase(md5);
      context->uploadingFlowIds().erase(md5);
    }
  }
  else if(Message::getType(j) == "download_block") {
    uint64_t flow_id = Message::getFlowIdFromDownLoadBlockMessage(j);
    Md5Info need_md5 = Message::getMD5FromDownLoadBlockMessage(j);
    SPDLOG_LOGGER_DEBUG(logger_, "download block {}", need_md5.getMd5Value());
    if(context->downloadingMd5s().find(need_md5) == context->downloadingMd5s().end()) {
      std::string mc = pathStorage().getMd5ContentFromStorage(need_md5);
      context->downloadingMd5s()[need_md5] = {1, std::move(mc)};
    }
    else {
      // add ref.
      context->downloadingMd5s()[need_md5].first += 1;
    }
    auto& download_context = context->downloadingContext();
    if(download_context.find(flow_id) != download_context.end()) {
      SPDLOG_LOGGER_CRITICAL(logger_, "replicate md5 on the same flow id : {}, {}", need_md5.getMd5Value(), flow_id);
      spdlog::shutdown();
      exit(-1);
    }
    else {
      //download_context[flow_id] = std::make_tuple<Md5Info, size_t, uint32_t>(need_md5, 0, 0);
      std::tuple<Md5Info, size_t, uint32_t> tmp(need_md5, 0, 0);
      download_context[flow_id] = tmp;
    }
    sendSomeMd5PieceToProxy(con);
  }
  else {
    SPDLOG_LOGGER_ERROR(logger_, "error state.");
    con->force_close();
    return;
  }

  end_point:
  con->get_next_message();
}

void StorageServer::onWriteComplete(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* context = con->get_context<StorageServerContext>();
  if(context->getState() == StorageServerContext::state::transfering_block_set) {
    sendSomeMd5sToProxy(con);
    SPDLOG_LOGGER_DEBUG(logger_, "continue to send block md5 sets.");
  }
  else if(context->getState() == StorageServerContext::state::working) {
    sendSomeMd5PieceToProxy(con);
  }
  else {
    SPDLOG_LOGGER_ERROR(logger_, "error state");
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
  SPDLOG_LOGGER_WARN(logger_, "on close, state : {}", con->get_state_str());
}

StorageServer::StorageServer(asio::io_context& io, std::string ip, std::string port, std::shared_ptr<spdlog::logger> logger):
                             client_(io, ip, port),
                             logger_(logger) {
  if(ds_.init() == false) {
    SPDLOG_LOGGER_CRITICAL(logger_, "hiredis init error.");
    spdlog::shutdown();
    exit(-1);
  }

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
  SPDLOG_LOGGER_DEBUG(logger_, "storage server start.");
}

void StorageServer::connectToProxyServer() {
  SPDLOG_LOGGER_DEBUG(logger_, "connect to proxy.");
  client_.connect();
}

void StorageServer::sendSomeMd5sToProxy(std::shared_ptr<TcpConnection> con) {
  StorageServerContext* state = con->get_context<StorageServerContext>();
  std::vector<Md5Info> now_trans;
  for(int i = 0; i < 128; ++i) {
    if(state->nextToTransferMd5Index() >= state->transferingMd5s().size()) {
      break;
    }
    now_trans.push_back(state->transferingMd5s()[state->nextToTransferMd5Index()]);
    ++state->nextToTransferMd5Index();
  }
  std::string message;
  if(state->nextToTransferMd5Index() >= state->transferingMd5s().size()) {
    message = Message::constructTransferBlockSetMessage(now_trans, true);
    state->nextToTransferMd5Index() = 0;
    state->transferingMd5s().clear();
    state->setState(StorageServerContext::state::working);
    SPDLOG_LOGGER_DEBUG(logger_, "transfer block set over. change state tu working.");
  }
  else {
    message = Message::constructTransferBlockSetMessage(now_trans, false);
  }
  con->send(std::move(message));
}
