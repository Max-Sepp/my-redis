#include "store/Hash.h"

size_t string_hash(const std::string& key) {
  constexpr std::hash<std::string> hasher;
  return hasher(key);
}

size_t int_hash(const int key) { return key; }
