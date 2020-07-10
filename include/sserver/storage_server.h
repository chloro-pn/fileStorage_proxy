#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include "common/asio_wrapper/client.h"
#include "spdlog/spdlog.h"
#include "path_storage.h"
#include "storage_server_context.h"
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

  void sendSomeMd5PieceToProxy(std::shared_ptr<TcpConnection> con);

  void cleanDownloadContext(StorageServerContext* context, uint64_t flow_id);

  void cleanUploadContext(StorageServerContext* context, uint64_t flow_id);

  PathStorage& pathStorage() {
    return ds_;
  }

private:
  Client client_;
  std::shared_ptr<spdlog::logger> logger_;
  PathStorage ds_;
};

#endif // STORAGE_SERVER_H
