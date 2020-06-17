#include "proxy/id_stroage.h"
#include "json.hpp"

using nlohmann::json;

IdStorage::IdStorage():context_(nullptr) {

}

bool IdStorage::init() {
  context_ = redisConnect("127.0.0.1", 6379);
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
  //check.
  freeReplyObject(reply);
}

bool IdStorage::findId(const Md5Info& id) const {
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HEXISTS id_md5s %s", id.getMd5Value().c_str()));
  if(reply->type != REDIS_REPLY_INTEGER) {
    //log fatal.
  }
  bool ret = (reply->integer == 1);
  freeReplyObject(reply);
  return ret;
}

std::vector<Md5Info> IdStorage::getMd5sFromId(const Md5Info& id) const {
  redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "HGET id_md5s %s", id.getMd5Value().c_str()));
  if(reply->type != REDIS_REPLY_STRING) {
    //log fatal.
  }
  std::vector<Md5Info> result;
  json j = json::parse(std::string(reply->str, reply->len));
  for(json::const_iterator it = j["md5s"].begin(); it != j["md5s"].end(); ++it) {
    result.push_back(Md5Info(it->get<std::string>()));
  }
  return result;
}




