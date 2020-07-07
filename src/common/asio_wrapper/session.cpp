#include "common/asio_wrapper/session.h"
#include "common/asio_wrapper/shared_buffer.h"
#include "common/asio_wrapper/util.h"
#include <cassert>
#include <spdlog/spdlog.h>

Session::Session(asio::io_context& io, tcp::socket socket) :
                 io_(io),
                 socket_(std::move(socket)),
                 length_(0),
                 writing_(false),
                 have_closed_(false),
                 tcp_connection_(std::make_shared<TcpConnection>(*this)) {

}

void Session::start() {
  onConnection();
  if(tcp_connection_->should_close()) {
    onClose();
  }
}

void Session::read_length() {
  auto self(shared_from_this());
  asio::async_read(socket_, asio::buffer(&length_, sizeof(length_)),
    [this, self](std::error_code ec, std::size_t length)->void {
      if (!ec) {
        length_ = util::networkToHost(length_);
        if(length_ > sizeof(data_) - 1) {  
          tcp_connection_->set_state(TcpConnection::state::lengthError);
          spdlog::get("console")->critical("length : {}", length_);
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

/*
void Session::do_write(const std::string& content) {
  //if async_writing_ == true.
  // push in write_bufs.
  // else
  // get all write_bufs.
  // async_writing_ = true.
  // async_write.
  // callback :
  // if write_bufs.empty() == false.
  //    continue to async_write and callback.
  // else
  //    on writecomplete.
  //    async_writing_ = false.
  auto self(shared_from_this());
  MultiBuffer bufs;
  //maybe need to check.
  uint32_t n = content.length();
  spdlog::get("console")->debug("length : {}", n);
  n = util::hostToNetwork(n);
  bufs.insert(std::string((const char*)(&n), sizeof(n)));
  bufs.insert(content);
  asio::async_write(socket_, bufs,
    [this, self](std::error_code ec, std::size_t length)->void {
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
*/
void Session::do_write(const std::string &content) {
  uint32_t n = content.size();
  n = util::hostToNetwork(n);
  write_bufs_.push_back(std::string((const char*)(&n), sizeof(n)));
  write_bufs_.push_back(content);
  if(writing_ == true) {
    spdlog::get("console")->trace("writing, return.");
    return;
  }
  else {
    MultiBuffer bufs;
    for(const auto&  each : write_bufs_) {
        bufs.insert(std::move(each));
    }
    write_bufs_.clear();
    writing_ = true;
    auto self(shared_from_this());
    asio::async_write(socket_, bufs, [this, self](std::error_code ec, std::size_t /* length */)->void {
      if(!ec) {
        assert(writing_ == true);
        continue_to_send();
      }
      else {
        tcp_connection_->set_state(TcpConnection::state::writeError);
        tcp_connection_->setError(ec.message());
        onClose();
      }
    });
  }
}

void Session::continue_to_send() {
  assert(writing_ == true);
  if(write_bufs_.empty() == true) {
    writing_ = false;
    onWriteComplete();
  }
  else {
    MultiBuffer bufs;
    for(const auto&  each : write_bufs_) {
        bufs.insert(std::move(each));
    }
    write_bufs_.clear();
    auto self(shared_from_this());
    asio::async_write(socket_, bufs, [this, self](std::error_code ec, std::size_t /* length */)->void {
      if(!ec) {
        assert(writing_ == true);
        continue_to_send();
      }
      else {
        tcp_connection_->set_state(TcpConnection::state::writeError);
        tcp_connection_->setError(ec.message());
        onClose();
      }
    });
  }
}

void Session::do_write(std::string&& content) {
  uint32_t n = content.size();
  n = util::hostToNetwork(n);
  write_bufs_.push_back(std::string((const char*)(&n), sizeof(n)));
  write_bufs_.push_back(std::move(content));
  if(writing_ == true) {
    return;
  }
  else {
    MultiBuffer bufs;
    for(const auto&  each : write_bufs_) {
      bufs.insert(std::move(each));
    }
    write_bufs_.clear();
    writing_ = true;
    auto self(shared_from_this());
    asio::async_write(socket_, bufs, [this, self](std::error_code ec, std::size_t /* length */)->void {
      if(!ec) {
        assert(writing_ == true);
        continue_to_send();
      }
      else {
        tcp_connection_->set_state(TcpConnection::state::writeError);
        tcp_connection_->setError(ec.message());
        onClose();
      }
    });
  }
}

void Session::run_after(size_t ms, std::function<void(std::shared_ptr<TcpConnection>)> func) {
  std::shared_ptr<timer_type> timer(new timer_type(io_));
  timer->expires_after(std::chrono::milliseconds(ms));
  std::weak_ptr<Session> self(shared_from_this());
  timer->async_wait(
    [func, self, timer](const std::error_code& error)->void {
      if(!error) {
        std::shared_ptr<Session> myself = self.lock();
        if(myself) {
          if(myself->closed() == true) {
            std::cerr << "closed before timer timeout." << std::endl;
            return;
          }
          func(myself->tcp_connection_);
        }
        else {
          return;
        }
      }
    }
  );
}
