#ifndef AGENT_H
#define AGENT_H

#include "common/asio_wrapper/client.h"
#include "common/asio_wrapper/tcp_connection.h"
#include "common/md5_info.h"
#include "asio.hpp"
#include <string>
#include <vector>

using asio::ip::tcp;

class Agent {
private:
  struct inner_type_ {
    Md5Info md5;
    size_t start_point;
    size_t length;
  };

public:
  Agent(asio::io_context& io, std::string ip, std::string port);

  void uploadFile(std::string path);

  std::vector<inner_type_> getMd5sFromFile(std::string filepath) {
    //just for test.
    return std::vector<inner_type_>();
  }

  std::vector<Md5Info> getMd5sFromInnerType(const std::vector<inner_type_>& md5s) {
    //just for test.
    return std::vector<Md5Info>();
  }

  void sendBlockPieceFromCurrentPoint(std::shared_ptr<TcpConnection> con) {
    //just for test.
  }

  void onConnection(std::shared_ptr<TcpConnection> con);

  void onMessage(std::shared_ptr<TcpConnection> con);

  void onWriteComplete(std::shared_ptr<TcpConnection> con);

  void onClose(std::shared_ptr<TcpConnection> con);

private:
  Client client_;
  std::string arg_;
  enum class doing { nothing, upload , download };
  doing what_;

  std::vector<inner_type_> file_md5s_info_;
  std::vector<Md5Info> upload_md5s_info_;
  size_t current_uploading_index_;
  size_t current_block_offset_;
};

#endif // AGENT_H
