#ifndef PROXY_H
#define PROXY_H

#include "common/asio_wrapper/server.h"
#include "common/md5_info.h"
#include "asio.hpp"
#include "json.hpp"
#include "id_stroage.h"
#include "client_context.h"
#include "spdlog/spdlog.h"
#include "md5.h"
#include <map>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include <cassert>

using nlohmann::json;

class Proxy {
public:
  Proxy(asio::io_context& io, uint16_t p1_port, uint16_t p2_port, std::shared_ptr<spdlog::logger> logger);

  void clientOnConnection(std::shared_ptr<TcpConnection> con);

  void clientOnMessage(std::shared_ptr<TcpConnection> con);

  void clientOnClose(std::shared_ptr<TcpConnection> con);

  void storageOnConnection(std::shared_ptr<TcpConnection> con);

  void storageOnMessage(std::shared_ptr<TcpConnection> con);

  void storageOnClose(std::shared_ptr<TcpConnection> con);

  IdStorage& idStorage() {
      return id_storage_;
  }

private:
  Server p1_server_;
  Server p2_server_;
  using FlowType = uint64_t;

  static FlowType get_flow_id();

  std::map<FlowType, std::shared_ptr<TcpConnection>> clients_;
  std::set<std::shared_ptr<TcpConnection>> storage_servers_;
  IdStorage id_storage_;
  std::shared_ptr<spdlog::logger> logger_;

  std::vector<std::shared_ptr<TcpConnection>> selectSuitableStorageServers() const;

  Md5Info getFileId(ClientContext* client) {
      std::string tmp;
      for(const auto& each : client->getUploadingBlockMd5s()) {
        tmp.append(each.getMd5Value());
      }
      return Md5Info(MD5(tmp).toStr());
  }

  void handleClientUploadRequestMessage(std::shared_ptr<TcpConnection> client, const json&);

  void handleClientUploadBlockMessage(std::shared_ptr<TcpConnection> client, json&);

  void handleClientUploadAllBlocksMessage(std::shared_ptr<TcpConnection> client, const json&);

  std::vector<Md5Info> getNeedUploadMd5s(const std::vector<Md5Info>& md5s);

  void handleClientDownloadRequestMessage(std::shared_ptr<TcpConnection> client, const json&);

  void requestBlockFromStorageServer(const Md5Info& md5, std::shared_ptr<TcpConnection> client);
};

#endif // PROXY_H
