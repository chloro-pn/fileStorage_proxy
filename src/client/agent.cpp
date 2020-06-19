#include "client/agent.h"
#include "client/agent_context.h"
#include "common/message.h"
#include "common/md5_info.h"
#include "json.hpp"
#include "unistd.h"
#include "spdlog/logger.h"
#include "spdlog/spdlog.h"
#include "md5.h"
#include <memory>
#include <cassert>
#include <algorithm>
#include <cstdlib> // for exit();

using nlohmann::json;

#define BLOCK_SIZE 64 * 1024 //64 kb for test.
#define EACH_MESSAGE_SEND 2048

std::vector<Agent::inner_type_> Agent::getMd5sFromFile(std::string filepath) {
  std::vector<inner_type_> result;
  fd_ = open(filepath.c_str(), O_RDONLY);
  if(fd_ < 0) {
    logger_->critical("file {} open error.", filepath);
    spdlog::shutdown();
    exit(-1);
  }
  size_t start_point = 0;
  char buf[BLOCK_SIZE + 1];
  while(true) {
    ssize_t n = read(fd_, buf, sizeof(buf) - 1);
    if(n < 0) {
      logger_->critical("file {} read error.", filepath);
      spdlog::shutdown();
      exit(-1);
    }
    else if(n == 0) {
      break;
    }
    else {
      buf[n] = '\0';
      MD5 md5(std::string(buf, n));
      logger_->trace("md5 str : {}", md5.toStr());
      result.push_back(inner_type_(Md5Info(md5.toStr()), start_point, n));
      start_point += n;
    }
  }
  return result;
}

std::vector<Md5Info> Agent::getMd5sFromInnerType(const std::vector<inner_type_> &md5s) {
  std::vector<Md5Info> result;
  for(auto& each : md5s) {
    result.push_back(each.md5);
    logger_->trace("md5 : {}", each.md5.getMd5Value());
  }
  return result;
}

void Agent::sendBlockPieceFromCurrentPoint(std::shared_ptr<TcpConnection> con) {
  //no block need to send.
  if(upload_md5s_info_.size() == 0) {
    AgentContext* context = con->get_context<AgentContext>();
    std::string message = Message::constructUploadAllBlocksMessage();
    con->send(message);
    context->setState(AgentContext::state::waiting_final_result);
    return;
  }
  assert(current_uploading_index_ < upload_md5s_info_.size());
  assert(current_block_offset_ < BLOCK_SIZE);
  auto it = std::find_if(file_md5s_info_.begin(), file_md5s_info_.end(), [this](const inner_type_& tmp)->bool {
    return upload_md5s_info_[current_uploading_index_].getMd5Value() == tmp.md5.getMd5Value();
  });
  assert(it != file_md5s_info_.end());
  size_t send_length = EACH_MESSAGE_SEND;
  bool eof = false;
  if(send_length + current_block_offset_ > it->length) {
    //for example.
    //length = 100, offset = 75, send_length = 30
    //so send_length = 100 - 75 = 25 byte.
    send_length = it->length - current_block_offset_;
    eof = true;
    AgentContext* context = con->get_context<AgentContext>();
    assert(context->getState() == AgentContext::state::uploading_blocks);
    context->setState(AgentContext::state::waiting_block_ack);
  }
  std::string content = getContentFromFile(*it, current_block_offset_, send_length);
  std::string message = Message::createUploadBlockMessage(it->md5, piece_index_, eof, std::move(content));
  con->send(std::move(message));
  current_block_offset_ += send_length;
  ++piece_index_;
}

std::string Agent::getContentFromFile(const inner_type_& it, size_t offset, size_t send_length) {
  lseek(fd_, it.start_point + offset, SEEK_SET);
  std::string str;
  str.resize(send_length + 1, '\0');
  read(fd_, &*(str.begin()), send_length);
  return str.c_str();
}

void Agent::onConnection(std::shared_ptr<TcpConnection> con) {
  con->set_context(std::make_shared<AgentContext>());
  AgentContext* context = con->get_context<AgentContext>();
  if(what_ == doing::upload) {
    file_md5s_info_ = getMd5sFromFile(arg_);
    std::string message = Message::createUploadRequestMessage(getMd5sFromInnerType(file_md5s_info_));
    con->send(message);
    logger_->trace("change state to waiting upload response.");
    context->setState(AgentContext::state::waiting_upload_response);
  }
}

void Agent::onMessage(std::shared_ptr<TcpConnection> con) {
  AgentContext* context = con->get_context<AgentContext>();
  json j = json::parse(std::string(con->message_data(), con->message_length()));

  if(context->getState() == AgentContext::state::waiting_upload_response) {
    if(Message::getType(j) == "upload_response") {
      upload_md5s_info_ = Message::getMd5FromUploadResponseMessage(j);
      current_uploading_index_ = 0;
      current_block_offset_ = 0;
      context->setState(AgentContext::state::uploading_blocks);
      //send and update state.
      sendBlockPieceFromCurrentPoint(con);
    }
    else {
      logger_->error("error state : {}", Message::getType(j));
      con->force_close();
      return;
    }
  }
  else if(context->getState() == AgentContext::state::waiting_block_ack) {
    if(Message::getType(j) == "upload_block_ack") {
      ++current_uploading_index_;
      current_block_offset_ = 0;
      piece_index_ = 0;
      if(current_uploading_index_ >= upload_md5s_info_.size()) {
        //send upload all blocks.
        std::string message = Message::constructUploadAllBlocksMessage();
        con->send(message);
        context->setState(AgentContext::state::waiting_final_result);
      }
      else {
        context->setState(AgentContext::state::uploading_blocks);
        sendBlockPieceFromCurrentPoint(con);
      }
    }
    else {
      logger_->error("error state : {}.", Message::getType(j));
      con->force_close();
    }
  }
  else if(context->getState() == AgentContext::state::waiting_final_result) {
    if(Message::getType(j) == "file_storage_succ") {
      //notify client.
      con->shutdownw();
      context->setState(AgentContext::state::succ);
    }
    else if(Message::getType(j) == "file_storage_fail") {
      //notify client.
      con->shutdownw();
      context->setState(AgentContext::state::fail);
    }
    else {
      logger_->error("error state : {}.", Message::getType(j));
      con->force_close();
    }
  }
}

void Agent::onWriteComplete(std::shared_ptr<TcpConnection> con) {
  logger_->trace("func : onWriteComplete.");
  AgentContext* context = con->get_context<AgentContext>();
  if(context->getState() != AgentContext::state::uploading_blocks) {
    return;
  }
  else {
    sendBlockPieceFromCurrentPoint(con);
  }
}

void Agent::onClose(std::shared_ptr<TcpConnection> con) {
  if(con->get_state() != TcpConnection::state::readZero) {
    logger_->error("error connection state : {}", con->get_state_str());
  }
  AgentContext* context = con->get_context<AgentContext>();
  if(context->getState() != AgentContext::state::succ) {
    logger_->error("error context state");
  }
  logger_->trace("client {} disconnect.");
}

Agent::Agent(asio::io_context& io, std::string ip, std::string port, std::shared_ptr<spdlog::logger> logger):
             client_(io, ip, port),
             what_(doing::nothing),
             current_uploading_index_(0),
             current_block_offset_(0),
             piece_index_(0),
             logger_(logger) {
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
  logger_->trace("agent start.");
}

void Agent::uploadFile(std::string path) {
  logger_->trace("upload file : {}.", path);
  assert(what_ == doing::nothing);
  what_ = doing::upload;
  arg_ = path;
  client_.connect();
}
