#ifndef MYREDIS_CLIENT_CONNECT_H_
#define MYREDIS_CLIENT_CONNECT_H_
#include <string>

namespace myredis {

int Connect(const std::string& hostname, const std::string& port);

}  // namespace myredis

#endif  // MYREDIS_CLIENT_CONNECT_H_
