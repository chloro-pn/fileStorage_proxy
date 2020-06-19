#ifndef IDSTORAGE_H
#define IDSTORAGE_H

#include "hiredis.h"
#include "common/md5_info.h"
#include "spdlog/spdlog.h"
#include <vector>

class IdStorage {
public:
  IdStorage();

  bool init();

  ~IdStorage();

  void storageIdMd5s(const Md5Info& id, const std::vector<Md5Info>& md5s);

  bool findId(const Md5Info& id) const ;

  std::vector<Md5Info> getMd5sFromId(const Md5Info& id) const;

private:
  redisContext* context_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // IDSTORAGE_H
