#include "store/hash.h"

#include <functional>

namespace myredis {

size_t StringHash(const std::string& key) {
  constexpr std::hash<std::string> hasher;
  return hasher(key);
}

size_t IntHash(const int key) { return key; }

}  // namespace myredis
