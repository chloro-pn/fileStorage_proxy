#include "common/asio_wrapper/util.h"
#include "arpa/inet.h"

namespace util {
uint16_t hostToNetwork(uint16_t n) {
  return htons(n);
}

uint16_t networkToHost(uint16_t n) {
  return ntohs(n);
}

uint32_t hostToNetwork(uint32_t n) {
  return htonl(n);
}

uint32_t networkToHost(uint32_t n) {
  return ntohl(n);
}
}
