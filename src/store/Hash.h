#ifndef MY_REDIS_HASH_H
#define MY_REDIS_HASH_H

#include <string>

size_t string_hash(const std::string& key);
size_t int_hash(int key);

#endif  // MY_REDIS_HASH_H
