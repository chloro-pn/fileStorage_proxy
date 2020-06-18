#ifndef AGENT_H
#define AGENT_H

#include "common/asio_wrapper/client.h"
#include "common/asio_wrapper/tcp_connection.h"
#include "common/md5_info.h"
#include "asio.hpp"
#include "spdlog/logger.h"
#include <string>
#include <vector>

using asio::ip::tcp;

class Agent {
private:
  struct inner_type_ {
    inner_type_(const Md5Info& m, size_t sp, size_t l):
      md5(m), start_point(sp), length(l) {

    }
    Md5Info md5;
    size_t start_point;
    size_t length;
  };

public:
  Agent(asio::io_context& io, std::string ip, std::string port, std::shared_ptr<spdlog::logger> logger);

  void uploadFile(std::string path);

  std::string getContentFromFile(const inner_type_& it, size_t offset, size_t send_length);

  std::vector<inner_type_> getMd5sFromFile(std::string filepath);

  std::vector<Md5Info> getMd5sFromInnerType(const std::vector<inner_type_>& md5s);

  void sendBlockPieceFromCurrentPoint(std::shared_ptr<TcpConnection> con);

  void onConnection(std::shared_ptr<TcpConnection> con);

  void onMessage(std::shared_ptr<TcpConnection> con);

  void onWriteComplete(std::shared_ptr<TcpConnection> con);

  void onClose(std::shared_ptr<TcpConnection> con);

private:
  Client client_;
  std::string arg_;
  int fd_;
  enum class doing { nothing, upload , download };
  doing what_;

  std::vector<inner_type_> file_md5s_info_;
  std::vector<Md5Info> upload_md5s_info_;
  size_t current_uploading_index_;
  size_t current_block_offset_;
  size_t piece_index_;

  std::shared_ptr<spdlog::logger> logger_;
};

#endif // AGENT_H
