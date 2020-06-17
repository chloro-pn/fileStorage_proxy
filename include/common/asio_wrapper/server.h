#ifndef SERVER_H
#define SERVER_H

#include "asio.hpp"
#include "session.h"
#include <memory>

using asio::ip::tcp;

class Server {
private:
  using callback = std::function<void(std::shared_ptr<TcpConnection> con)>;

public:
  Server(asio::io_context& io_context, short port);

  void setOnMessage(const callback& cb);

  void setOnConnection(const callback& cb);

  void setOnClose(const callback& cb);

  void setOnWriteComplete(const callback& cb);

private:
  void do_accept();

  tcp::acceptor acceptor_;

  callback on_message_;
  callback on_connection_;
  callback on_close_;
  callback on_write_complete_;

  asio::io_context& io_;
};

#endif // SERVER_H
