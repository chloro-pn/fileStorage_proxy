#ifndef DISK_STORAGE_H
#define DISK_STORAGE_H

#include "common/md5_info.h"
#include "hiredis.h"
#include <vector>

class PathStorage {
public:
  PathStorage();

  bool init();

  std::vector<Md5Info> getAllItems() const;

  void storageItemPath(const Md5Info& md5, std::string path) {
    //just for test.
  }

  bool checkItem(const Md5Info& md5) const {
    //just for test.
    return true;
  }

  void cleanItem(const Md5Info& md5) {
    //just for test.
  }

  std::string getPath(const Md5Info& md5) const {
    //just for test.
    return std::string();
  }

  bool existItem(const Md5Info& md5) const {
    //just for test.
    return true;
  }

  ~PathStorage();

private:
  redisContext* context_;
};

#endif // DISK_STORAGE_H
