#include "common/asio_wrapper/server.h"

Server::Server(asio::io_context& io_context, short port) :
               acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
               io_(io_context) {
  do_accept();
}

void Server::do_accept()
{
  acceptor_.async_accept(
    [this](std::error_code ec, tcp::socket socket)->void {
      if (!ec) {
        auto s = std::make_shared<Session>(io_, std::move(socket));
        s->setOnConnection(on_connection_);
        s->setOnMessage(on_message_);
        s->setOnWriteComplete(on_write_complete_);
        s->setOnClose(on_close_);
        s->start();
      }
      do_accept();
    }
  );
}

void Server::setOnMessage(const callback& cb) {
  on_message_ = cb;
}

void Server::setOnConnection(const callback& cb) {
  on_connection_ = cb;
}

void Server::setOnClose(const callback& cb) {
  on_close_ = cb;
}

void Server::setOnWriteComplete(const callback& cb) {
  on_write_complete_ = cb;
}
