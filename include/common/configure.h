#ifndef CONFIGURE_H
#define CONFIGURE_H

#include "json.hpp"
#include "spdlog/spdlog.h"
#include <string>

using nlohmann::json;

class Configure {
public:
  void load(std::string conf_path);

  static Configure& instance();

  template<typename T>
  T get(const char* attr) const {
    return content_[attr].get<T>();
  }

  const std::string& getRedisIp() const {
    return redis_ip_;
  }

  uint16_t getRedisPort() const {
    return redis_port_;
  }

  uint16_t getClientPort() const {
    return client_port_;
  }

  uint16_t getStorageServerPort() const {
    return storage_server_port_;
  }

  short getBlockBackupCount() const {
    return block_backup_count_;
  }

private:
  Configure();

  std::string redis_ip_;
  uint16_t redis_port_;
  uint16_t client_port_;
  uint16_t storage_server_port_;
  short block_backup_count_;

  json content_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // CONFIGURE_H
