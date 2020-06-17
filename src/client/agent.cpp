#include "client/agent.h"
#include "client/agent_context.h"
#include "common/message.h"
#include "json.hpp"
#include <memory>
#include <cassert>
#include <iostream>

using nlohmann::json;

void Agent::onConnection(std::shared_ptr<TcpConnection> con) {
  con->set_context(std::make_shared<AgentContext>());
  AgentContext* context = con->get_context<AgentContext>();
  if(what_ == doing::upload) {
    file_md5s_info_ = getMd5sFromFile(arg_);
    std::string message = Message::createUploadRequestMessage(getMd5sFromInnerType(file_md5s_info_));
    con->send(message);
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
      std::cerr << "error state : " << Message::getType(j) << std::endl;
      con->force_close();
      return;
    }
  }
  else if(context->getState() == AgentContext::state::uploading_blocks) {
    if(Message::getType(j) == "upload_block_ack") {
      ++current_uploading_index_;
      current_uploading_index_ = 0;
      if(current_block_offset_ >= upload_md5s_info_.size()) {
        //send upload all blocks.
        std::string message = Message::constructUploadAllBlocksMessage();
        con->send(message);
        context->setState(AgentContext::state::waiting_final_result);
      }
      else {
        sendBlockPieceFromCurrentPoint(con);
      }
    }
    else {
      std::cerr << "error state : " << Message::getType(j) << std::endl;
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
      std::cerr << "error state : " << Message::getType(j) << std::endl;
      con->force_close();
    }
  }
}

void Agent::onWriteComplete(std::shared_ptr<TcpConnection> con) {
  AgentContext* context = con->get_context<AgentContext>();
  if(context->getState() != AgentContext::state::uploading_blocks) {
    //log error.
    con->force_close();
  }
  sendBlockPieceFromCurrentPoint(con);
}

void Agent::onClose(std::shared_ptr<TcpConnection> con) {
  if(con->get_state() != TcpConnection::state::readZero) {
    //log.
  }
  AgentContext* context = con->get_context<AgentContext>();
  if(context->getState() != AgentContext::state::succ) {
    //log.
  }
}

Agent::Agent(asio::io_context& io, std::string ip, std::string port):
             client_(io, ip, port),
             what_(doing::nothing),
             current_uploading_index_(0),
             current_block_offset_(0) {
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

void Agent::uploadFile(std::string path) {
  assert(what_ == doing::nothing);
  what_ = doing::upload;
  arg_ = path;
  client_.connect();
}
