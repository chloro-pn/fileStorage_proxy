#include "common/asio_wrapper/tcp_connection.h"
#include "common/asio_wrapper/session.h"

TcpConnection::TcpConnection(Session& sess):
                             session_(sess),
                             error_(""),
                             iport_(),
                             state_(state::goOn){
  auto lip = session_.socket_.local_endpoint().address().to_string();
  auto lport = session_.socket_.local_endpoint().port();
  auto pip = session_.socket_.remote_endpoint().address().to_string();
  auto pport = session_.socket_.remote_endpoint().port();
  iport_.reserve(64);
  iport_.append(lip);
  iport_.append(":");
  iport_.append(std::to_string(lport));
  iport_.append("->");
  iport_.append(pip);
  iport_.append(":");
  iport_.append(std::to_string(pport));
}

void TcpConnection::send(const std::string& str) {
  session_.do_write(str);
}

void TcpConnection::send(std::string&& str) {
  session_.do_write(str);
  //session_.do_write(std::move(str));
}

void TcpConnection::shutdownw() {
  asio::error_code ec;
  session_.socket_.shutdown(asio::socket_base::shutdown_send, ec);
  if(!ec) {
    return;
  }
  else {
    set_state(state::shutdownError);
    error_ = ec.message();
  }
}

void TcpConnection::run_after(size_t ms, std::function<void(std::shared_ptr<TcpConnection>)> self) {
  session_.run_after(ms, self);
}

const char* TcpConnection::message_data() const {
  return session_.data_;
}

size_t TcpConnection::message_length() const {
  return session_.length_;
}

void TcpConnection::force_close() {
  set_state(state::forceClose);
}

bool TcpConnection::should_close() const {
  return state_ != state::goOn;
}

const std::string& TcpConnection::iport() const {
  return iport_;
}

TcpConnection::state TcpConnection::get_state() const {
  return state_;
}

const char* TcpConnection::get_state_str() const {
  if(state_ == state::goOn) {
    return "go on";
  }
  else if(state_ == state::readZero) {
    return "read zero";
  }
  else if(state_ == state::outOfData) {
    return "out of data";
  }
  else if(state_ == state::readError) {
    return "read error";
  }
  else if(state_ == state::forceClose) {
    return "force close";
  }
  else if(state_ == state::writeError) {
    return "write error";
  }
  else if(state_ == state::lengthError) {
    return "length error";
  }
  else if(state_ == state::shutdownError) {
    return "shutdown error";
  }
  else {
    return "unknow error";
  }
}

void TcpConnection::set_state(state s) {
  state_ = s;
}

const std::string& TcpConnection::what() const {
  return error_;
}

void TcpConnection::set_context(std::shared_ptr<void> cxt) {
  context_ = cxt;
}

