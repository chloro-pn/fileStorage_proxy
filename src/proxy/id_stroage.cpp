#include "proxy/id_stroage.h"
#include "json.hpp"
#include "common/configure.h"

using nlohmann::json;

IdStorage::IdStorage():context_(nullptr),
                       logger_(spdlog::get("console")){

}

bool IdStorage::init() {
  std::string ip = Configure::instance().get<std::string>("redis_ip");
  uint16_t port = Configure::instance().get<uint16_t>("redis_port");

  context_ = redisConnect(ip.c_str(), port);
  if(context_ == nullptr || context_->err) {
    return false;
  }
  return true;
}

IdStorage::~IdStorage() {
  if(context_ != nullptr) {
    redisFree(context_);
  }
}

void IdStorage::storageIdMd5s(const Md5Info& id, const std::vector<Md5Info>& md5s) {
  json j;
  for(auto& each : md5s) {
    j["md5s"].push_back(each.getMd5Value());
  }
  std::string value = j.dump();
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HSET id_md5s, %s %s", id.getMd5Value().c_str(), value.c_str()));
  if(reply->type != REDIS_REPLY_INTEGER) {
    logger_->critical("error reply type : {}", reply->type);
    spdlog::shutdown();
    exit(-1);
  }
  freeReplyObject(reply);
}

bool IdStorage::findId(const Md5Info& id) const {
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HEXISTS id_md5s %s", id.getMd5Value().c_str()));
  if(reply->type != REDIS_REPLY_INTEGER) {
    logger_->critical("error reply type : {}", reply->type);
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
    logger_->critical("error reply type : {}", reply->type);
    spdlog::shutdown();
    exit(-1);
  }
  std::vector<Md5Info> result;
  json j = json::parse(std::string(reply->str, reply->len));
  for(json::const_iterator it = j["md5s"].begin(); it != j["md5s"].end(); ++it) {
    result.push_back(Md5Info(it->get<std::string>()));
  }
  return result;
}
