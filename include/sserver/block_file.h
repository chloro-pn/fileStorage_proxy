#ifndef BLOCK_FILE_H
#define BLOCK_FILE_H

#include <string>
#include "spdlog/spdlog.h"

class BlockFile {
public:
  BlockFile();

  bool openExistfile(std::string path);

  bool createNewFile(std::string path);

  ~BlockFile();

  int fd();
  
  static bool fileExist(std::string file_path);
  
  std::string readBlock() const;

  void writeBlock(const std::string& str);

private:
  int fd_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // BLOCK_FILE_H
