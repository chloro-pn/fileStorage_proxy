#include "sserver/block_file.h"
#include <unistd.h>
#include <fcntl.h>

BlockFile::BlockFile():fd_(-1),
                       logger_(spdlog::get("console")){

}

bool BlockFile::createNewFile(std::string path) {
  fd_ = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL);
  if(fd_ < 0) {
    logger_->error("file open error");
    return false;
  }
  return true;
}

bool BlockFile::openExistfile(std::string path) {
  fd_ = open(path.c_str(), O_RDONLY);
  if(fd_ < 0) {
    logger_->error("file open error.");
    return false;
  }
  return true;
}

std::string BlockFile::readBlock() const {
  std::string result;
  while(true) {
    char buf[1025] = {0};
    size_t n = read(fd_, buf, sizeof(buf) - 1);
    if(n < 0) {
      logger_->critical("file read error.");
      spdlog::shutdown();
      exit(-1);
    }
    else if(n == 0) {
      break;
    }
    else {
      buf[n] = '\0';
      result.append(std::string(buf, n));
    }
  }
  return result;
}

void BlockFile::writeBlock(const std::string &str){
  size_t n = write(fd_, str.data(), str.length());
  if(n != str.length()) {
    logger_->critical("file write error.");
    spdlog::shutdown();
    exit(-1);
  }
}

BlockFile::~BlockFile() {
  if(fd_ >= 0) {
    close(fd_);
  }
}
