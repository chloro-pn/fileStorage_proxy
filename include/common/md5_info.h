#ifndef MD5_INFO_H
#define MD5_INFO_H

#include <string>
#include <cassert>

class Md5Info {
public:
  Md5Info() = default;

  Md5Info(const char* ptr, size_t n):md5_value_(ptr, n) {
    assert(n == 32);
  }

  explicit Md5Info(const std::string& str):md5_value_(str) {
    assert(str.length() == 32);
  }

  const std::string& getMd5Value() const {
    return md5_value_;
  }

  bool operator<(const Md5Info& other) const {
    return md5_value_ < other.md5_value_;
  }

private:
    std::string md5_value_;
};

#endif // MD5_INFO_H
