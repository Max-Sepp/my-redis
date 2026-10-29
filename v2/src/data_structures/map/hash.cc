#include "data_structures/map/hash.h"

#include <cstddef>
#include <functional>
#include <string>

namespace myredis {

size_t StringHash(const std::string& key) {
  constexpr std::hash<std::string> hasher;
  return hasher(key);
}

size_t IntHash(const int key) { return key; }

}  // namespace myredis
