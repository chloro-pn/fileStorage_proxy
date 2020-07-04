#ifndef SESSION_H
#define SESSION_H

#include "asio.hpp"
#include "tcp_connection.h"
#include "timer.h"
#include <functional>
#include <memory>

using asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
  friend class TcpConnection;
  using callback = std::function<void(std::shared_ptr<TcpConnection> con)>;

  Session(asio::io_context& io, tcp::socket socket);

  void start();

  void setOnMessage(const callback& cb) {
    on_message_ = cb;
  }

  void setOnConnection(const callback& cb) {
    on_connection_ = cb;
  }

  void setOnClose(const callback& cb) {
    on_close_ = cb;
  }

  void setOnWriteComplete(const callback& cb) {
    on_write_complete_ = cb;
  }

  void run_after(size_t ms, std::function<void(std::shared_ptr<TcpConnection>)> func);

private:
  void read_length();

  void read_body();

  void do_write(const std::string& content);

  void do_write(std::string&& content);

  asio::io_context& io_;
  tcp::socket socket_;

  uint32_t length_;
  char data_[65535];

  callback on_message_;
  callback on_connection_;
  callback on_close_;
  bool have_closed_;
  callback on_write_complete_;

  std::shared_ptr<TcpConnection> tcp_connection_;

  void onMessage() {
    if(on_message_) {
      on_message_(tcp_connection_);
    }
  }

  void onConnection() {
    if(on_connection_) {
      on_connection_(tcp_connection_);
    }
  }

  void onClose() {
    if(have_closed_ == true) {
      return;
    }
    have_closed_ = true;
    if(on_close_) {
      on_close_(tcp_connection_);
    }
    socket_.close();
  }

  void onWriteComplete() {
    if(on_write_complete_) {
      on_write_complete_(tcp_connection_);
    }
  }
};

#endif // SESSION_H
