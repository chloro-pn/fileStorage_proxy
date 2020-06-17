#ifndef CLIENT_H
#define CLIENT_H

#include "tcp_connection.h"
#include "asio.hpp"
#include <string>
#include <functional>

using asio::ip::tcp;

class Client {
private:
  using callback = std::function<void(std::shared_ptr<TcpConnection> con)>;

public:
  Client(asio::io_context& io, std::string ip, std::string port);

  void connect();

  void setOnMessage(const callback& cb);

  void setOnConnection(const callback& cb);

  void setOnClose(const callback& cb);

  void setOnWriteComplete(const callback& cb);

private:
  asio::io_context& io_;
  tcp::resolver::results_type ret_;
  tcp::socket socket_;

  callback on_connection_;
  callback on_message_;
  callback on_write_complete_;
  callback on_close_;
};

#endif // CLIENT_H
