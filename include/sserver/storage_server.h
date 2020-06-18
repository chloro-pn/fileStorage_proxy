#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include "common/asio_wrapper/client.h"
#include "spdlog/spdlog.h"
#include "asio.hpp"

using asio::ip::tcp;

class StorageServer {
public:
  StorageServer(asio::io_context& io, std::string ip, std::string port, std::shared_ptr<spdlog::logger> logger);

  void connectToProxyServer();

  void sendSomeMd5sToProxy(std::shared_ptr<TcpConnection> con);

  void onConnection(std::shared_ptr<TcpConnection> con);

  void onMessage(std::shared_ptr<TcpConnection> con);

  void onWriteComplete(std::shared_ptr<TcpConnection> con);

  void onClose(std::shared_ptr<TcpConnection> con);

private:
  Client client_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // STORAGE_SERVER_H
