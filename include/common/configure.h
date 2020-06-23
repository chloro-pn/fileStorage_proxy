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

private:
  Configure();

  json content_;
  std::shared_ptr<spdlog::logger> logger_;
};

#endif // CONFIGURE_H
