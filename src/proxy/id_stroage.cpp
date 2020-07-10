#include "proxy/id_stroage.h"
#include "json.hpp"
#include "common/configure.h"

using nlohmann::json;

IdStorage::IdStorage():context_(nullptr),
                       logger_(spdlog::get("console")){

}

bool IdStorage::init() {
  std::string ip = Configure::instance().getRedisIp();
  uint16_t port = Configure::instance().getRedisPort();

  context_ = redisConnect(ip.c_str(), port);
  if(context_ == nullptr || context_->err) {
    return false;
  }
  SPDLOG_LOGGER_INFO(logger_, "redis connect succ.");
  return true;
}

IdStorage::~IdStorage() {
  if(context_ != nullptr) {
    SPDLOG_LOGGER_INFO(logger_, "redis disconnect.");
    redisFree(context_);
  }
}

void IdStorage::storageIdMd5s(const Md5Info& id, const std::vector<Md5Info>& md5s) {
  json j;
  for(auto& each : md5s) {
    j["md5s"].push_back(each.getMd5Value());
  }
  std::string value = j.dump();
  SPDLOG_LOGGER_DEBUG(logger_, "store : {} -> {}", id.getMd5Value(), value);
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HSET id_md5s %s %s", id.getMd5Value().c_str(), value.c_str()));
  if(reply->type != REDIS_REPLY_INTEGER) {
    SPDLOG_LOGGER_CRITICAL(logger_, "error reply type : {}", reply->type);
    spdlog::shutdown();
    exit(-1);
  }
  freeReplyObject(reply);
}

bool IdStorage::findId(const Md5Info& id) const {
  SPDLOG_LOGGER_DEBUG(logger_, "find : {}", id.getMd5Value());
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HEXISTS id_md5s %s", id.getMd5Value().c_str()));
  if(reply->type != REDIS_REPLY_INTEGER) {
    SPDLOG_LOGGER_CRITICAL(logger_, "error reply type : {}", reply->type);
    spdlog::shutdown();
    exit(-1);
  }
  bool ret = (reply->integer == 1);
  freeReplyObject(reply);
  return ret;
}

std::vector<Md5Info> IdStorage::getMd5sFromId(const Md5Info& id) const {
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HGET id_md5s %s", id.getMd5Value().c_str()));
  if(reply->type != REDIS_REPLY_STRING) {
    SPDLOG_LOGGER_CRITICAL(logger_, "error reply type : {}", reply->type);
    spdlog::shutdown();
    exit(-1);
  }
  std::vector<Md5Info> result;
  json j = json::parse(std::string(reply->str, reply->len));
  SPDLOG_LOGGER_DEBUG(logger_, "get md5s {} from id {}", std::string(reply->str, reply->len), id.getMd5Value());
  for(json::const_iterator it = j["md5s"].begin(); it != j["md5s"].end(); ++it) {
    result.push_back(Md5Info(it->get<std::string>()));
  }
  return result;
}
