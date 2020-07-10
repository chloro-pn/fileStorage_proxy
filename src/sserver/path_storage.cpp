#include "sserver/path_storage.h"
#include "hiredis.h"
#include <iostream>

PathStorage::PathStorage():context_(nullptr),
                           logger_(spdlog::get("console")) {

}

bool PathStorage::init() {
  context_ = redisConnect("127.0.0.1", 6379);
  if(context_ == nullptr || context_->err) {
    return false;
  }
  return true;
}

std::vector<Md5Info> PathStorage::getAllItems() const {
  std::vector<Md5Info> result;
  redisReply *reply = nullptr;
  reply = static_cast<redisReply*>(redisCommand(context_, "HGETALL md5_path"));
  if(reply->type != REDIS_REPLY_ARRAY) {
    SPDLOG_LOGGER_CRITICAL(logger_, "redis reply error type : {}", reply->type);
    spdlog::shutdown();
    exit(-1);
  }
  for(size_t i = 0; i < reply->elements; i += 2) {
    result.push_back(Md5Info(std::string(reply->element[i]->str, reply->element[i]->len)));
  }
  freeReplyObject(reply);
  return result;
}

PathStorage::~PathStorage() {
  if(context_ != nullptr) {
    redisFree(context_);
  }
}
