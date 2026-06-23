#ifndef MYREDIS_STORE_HASH_H_
#define MYREDIS_STORE_HASH_H_

#include <cstddef>
#include <string>

namespace myredis {

size_t StringHash(const std::string& key);
size_t IntHash(int key);

}  // namespace myredis

#endif  // MYREDIS_STORE_HASH_H_
