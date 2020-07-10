#include "sserver/block_file.h"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <cerrno>

BlockFile::BlockFile():fd_(-1),
                       logger_(spdlog::get("console")){

}

bool BlockFile::createNewFile(std::string path) {
  fd_ = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  if(fd_ < 0) {
    SPDLOG_LOGGER_ERROR(logger_, "file open error : {}", strerror(errno));
    return false;
  }
  return true;
}

bool BlockFile::openExistfile(std::string path) {
  fd_ = open(path.c_str(), O_RDONLY);
  if(fd_ < 0) {
    SPDLOG_LOGGER_ERROR(logger_, "file open error : {}", strerror(errno));
    return false;
  }
  return true;
}

bool BlockFile::fileExist(std::string file_path) {
  int result = access(file_path.c_str(), F_OK | R_OK);
  return result == 0;
}

std::string BlockFile::readBlock() const {
  std::string result;
  while(true) {
    char buf[1025] = {0};
    size_t n = read(fd_, buf, sizeof(buf) - 1);
    if(n < 0) {
      SPDLOG_LOGGER_ERROR(logger_, "file read error : {}", strerror(errno));
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
    SPDLOG_LOGGER_ERROR(logger_, "file write error : {}", strerror(errno));
    spdlog::shutdown();
    exit(-1);
  }
}

BlockFile::~BlockFile() {
  if(fd_ >= 0) {
    close(fd_);
  }
}
