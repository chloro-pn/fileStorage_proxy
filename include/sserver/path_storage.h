#ifndef DISK_STORAGE_H
#define DISK_STORAGE_H

#include "common/md5_info.h"
#include "spdlog/spdlog.h"
#include "hiredis.h"
#include <vector>

class PathStorage {
public:
  PathStorage();

  bool init();

  std::vector<Md5Info> getAllItems() const;

  void storageItemPath(const Md5Info& md5, std::string path) {
    redisReply* reply = nullptr;
    reply = static_cast<redisReply*>(redisCommand(context_, "HSET md5_path %s %s", md5.getMd5Value().c_str(), path.c_str()));
    if(reply->type != REDIS_REPLY_INTEGER) {
      logger_->critical("reids reply error type : {}", reply->type);
      spdlog::shutdown();
      exit(-1);
    }
    freeReplyObject(reply);
  }

  bool checkItem(const Md5Info& md5) const {
    redisReply* reply = nullptr;
    reply = static_cast<redisReply*>(redisCommand(context_, "HEXISTS md5_path %s", md5.getMd5Value().c_str()));
    if(reply->type != REDIS_REPLY_INTEGER) {
      logger_->critical("reids reply error type : {}", reply->type);
      spdlog::shutdown();
      exit(-1);
    }
    return reply->integer == 1;
  }

  void cleanItem(const Md5Info& md5) {
    redisReply* reply = nullptr;
    reply = static_cast<redisReply*>(redisCommand(context_, "HDEL md5_path %s", md5.getMd5Value().c_str()));
    if(reply->type != REDIS_REPLY_INTEGER) {
      logger_->critical("reids reply error type : {}", reply->type);
      spdlog::shutdown();
      exit(-1);
    }
  }

  std::string getPath(const Md5Info& md5) const {
    redisReply* reply = nullptr;
    reply = static_cast<redisReply*>(redisCommand(context_, "HGET md5_path %s", md5.getMd5Value().c_str()));
    if(reply->type != REDIS_REPLY_STRING) {
      logger_->critical("reids reply error type : {}", reply->type);
      spdlog::shutdown();
      exit(-1);
    }
    return std::string(reply->str, reply->len);
  }

  ~PathStorage();

private:
  redisContext* context_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // DISK_STORAGE_H
