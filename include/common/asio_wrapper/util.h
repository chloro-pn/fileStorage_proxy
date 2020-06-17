#ifndef UTIL_H
#define UTIL_H

#include <cstdint>

namespace util {
uint16_t hostToNetwork(uint16_t n);

uint16_t networkToHost(uint16_t n);

uint32_t hostToNetwork(uint32_t n);

uint32_t networkToHost(uint32_t n);
} // namespace util
#endif // UTIL_H
