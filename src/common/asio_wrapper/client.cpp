#include "common/asio_wrapper/client.h"
#include "common/asio_wrapper/session.h"
#include "asio.hpp"

using asio::ip::tcp;

Client::Client(asio::io_context& io, std::string ip, std::string port):
              io_(io),
              socket_(io) {
  tcp::resolver resolver(io_);
  ret_ = resolver.resolve(ip, port);
}

void Client::connect() {
  asio::async_connect(socket_, ret_, [this](std::error_code ec, tcp::endpoint)->void {
    if(!ec) {
      auto s = std::make_shared<Session>(io_, std::move(socket_));
      s->setOnConnection(on_connection_);
      s->setOnMessage(on_message_);
      s->setOnWriteComplete(on_write_complete_);
      s->setOnClose(on_close_);
      s->start();
    }
  });
}

void Client::setOnMessage(const callback& cb) {
  on_message_ = cb;
}

void Client::setOnConnection(const callback& cb) {
  on_connection_ = cb;
}

void Client::setOnClose(const callback& cb) {
  on_close_ = cb;
}

void Client::setOnWriteComplete(const callback& cb) {
  on_write_complete_ = cb;
}
