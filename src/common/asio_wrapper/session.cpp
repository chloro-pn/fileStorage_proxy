#include "common/asio_wrapper/session.h"
#include "common/asio_wrapper/shared_buffer.h"
#include "common/asio_wrapper/util.h"
#include <cassert>
#include <spdlog/spdlog.h>

Session::Session(asio::io_context& io, tcp::socket socket) :
                 io_(io),
                 socket_(std::move(socket)),
                 length_(0),
                 have_closed_(false),
                 tcp_connection_(std::make_shared<TcpConnection>(*this)) {

}

void Session::start() {
  onConnection();
  if(tcp_connection_->should_close()) {
    onClose();
  }
  else {
    read_length();
  }
}

void Session::read_length() {
  auto self(shared_from_this());
  asio::async_read(socket_, asio::buffer(&length_, sizeof(length_)),
    [this, self](std::error_code ec, std::size_t length)->void {
      if (!ec) {
        length_ = util::networkToHost(length_);
        if(length_ > sizeof(data_) - 1) {
          spdlog::get("console")->debug("length : {} ", length_);	  
          tcp_connection_->set_state(TcpConnection::state::lengthError);
          onClose();
        }
        else {
          read_body();
        }
      }
      else {
        if(ec == asio::error::eof) {
          tcp_connection_->set_state(TcpConnection::state::readZero);
        }
        else {
          tcp_connection_->set_state(TcpConnection::state::readError);
        }
        tcp_connection_->setError(ec.message());
        onClose();
      }
    });
}

void Session::read_body() {
  auto self(shared_from_this());
  asio::async_read(socket_, asio::buffer(data_, length_),
    [this, self](std::error_code ec, std::size_t length)->void {
      if (!ec) {
        assert(length_ == length);
        onMessage();
        if(tcp_connection_->should_close()) {
          onClose();
        }
        else {
          read_length();
        }
      }
      else {
        if(ec == asio::error::eof) {
          if(length == length_) {
            onMessage();
            if(tcp_connection_->should_close()) {
              onClose();
              return;
            }
          }
          tcp_connection_->set_state(TcpConnection::state::readZero);
        }
        else {
          tcp_connection_->set_state(TcpConnection::state::readError);
        }
        tcp_connection_->setError(ec.message());
        onClose();
      }
    }
  );
}

void Session::do_write(const std::string& content) {
  auto self(shared_from_this());
  MultiBuffer bufs;
  //maybe need to check.
  uint32_t n = content.length();
  n = util::hostToNetwork(n);
  bufs.insert(std::string((const char*)(&n), sizeof(n)));
  bufs.insert(content);
  asio::async_write(socket_, bufs,
    [this, self](std::error_code ec, std::size_t /*length*/)->void {
      if (!ec) {
        onWriteComplete();
        if(tcp_connection_->should_close()) {
          onClose();
        }
      }
      else {
        tcp_connection_->set_state(TcpConnection::state::writeError);
        tcp_connection_->setError(ec.message());
        onClose();
      }
    }
  );
}

void Session::do_write(std::string&& content) {
  auto self(shared_from_this());
  MultiBuffer bufs;
  //maybe need to check.
  uint32_t n = content.length();
  n = util::hostToNetwork(n);
  bufs.insert(std::string((const char*)(&n), sizeof(n)));
  bufs.insert(std::move(content));
  asio::async_write(socket_, bufs,
    [this, self](std::error_code ec, std::size_t /*length*/)->void {
      if (!ec) {
        onWriteComplete();
        if(tcp_connection_->should_close()) {
          onClose();
        }
      }
      else {
        tcp_connection_->set_state(TcpConnection::state::writeError);
        tcp_connection_->setError(ec.message());
        onClose();
      }
    }
  );
}

void Session::run_after(size_t ms, std::function<void(std::shared_ptr<TcpConnection>)> func) {
  std::shared_ptr<timer_type> timer(new timer_type(io_));
  timer->expires_after(std::chrono::milliseconds(ms));
  std::weak_ptr<Session> self(shared_from_this());
  timer->async_wait(
    [func, self, timer](const std::error_code& error)->void {
      if(!error) {
        std::cout << "wake up!" << std::endl;
        std::shared_ptr<Session> myself = self.lock();
        if(myself) {
          func(myself->tcp_connection_);
        }
        else {
          std::cout << "have been closed." << std::endl;
          return;
        }
      }
    }
  );
}
