#include "asio.hpp"
#include <iostream>
#include <string>
#include "common/asio_wrapper/util.h"
#include "common/message.h"
#include "json.hpp"

using asio::ip::tcp;
using namespace nlohmann;

void send_message(tcp::socket& socket, const std::string msg) {
  uint32_t length = msg.size();
  length = util::hostToNetwork(length);
  std::string lth((char*)(&length), sizeof(length));
  asio::write(socket, asio::buffer(lth));
  asio::write(socket, asio::buffer(msg));
}

std::string read_message(tcp::socket& socket) {
  uint32_t length;
  std::string lth;
  lth.resize(sizeof(length));
  asio::read(socket, asio::buffer(lth));
  memcpy(&length, lth.data(), lth.size());
  length = util::networkToHost(length);
  std::cout << "message size : " << length << std::endl;
  std::string msg;
  msg.resize(length);
  asio::read(socket, asio::buffer(msg));
  std::cout << "read succ " << std::endl;
  return msg;
}

int main(int argc, const char* argv[]) {
  if(argc != 2) {
    std::cerr << "need one arg : download file id. " << std::endl;
    return -1;
  }
  asio::io_context io_context;
  tcp::socket sock(io_context);
  asio::ip::address_v4 address = asio::ip::address_v4::from_string("127.0.0.1");
  tcp::endpoint ep(address, 12345);
  std::error_code ec;
  sock.connect(ep, ec);
  if(ec) {
    std::cerr << ec.message() << std::endl;
    return -1;
  }
  std::cout << "connect succ. " << std::endl;
  std::string message = Message::constructDownLoadRequestMessage(Md5Info(argv[1]), {});
  send_message(sock, message);
  std::cout << "send succ. " << std::endl;

  std::string content;
  while(true) {
    message = read_message(sock);
    json j = json::parse(message);
    if(Message::getType(j) == "transfer_all_blocks") {
      break;
    }
    if(Message::getType(j) != "transfer_block") {
      std::cerr << "error message type : " << Message::getType(j) << std::endl;
      return -1;
    }
    content.append(Message::getContentFromTransferBlockMessage(j));
    std::cout << "current content : " << content << std::endl;
    if(Message::theLastBlockPiece(j) == true) {
      std::cout << "the last piece" << std::endl;
      Md5Info md5 = Message::getMd5FromTransferBlockMessage(j);
      std::cout << "md5 transfer ack : " << md5.getMd5Value() << std::endl;
      message = Message::constructTransferBlockAckMessage(md5);
      send_message(sock, message);
      std::cout << "send succ. " << std::endl;
    }
  }
  std::cout << "get file : " << content << std::endl;
  return 0;
}
