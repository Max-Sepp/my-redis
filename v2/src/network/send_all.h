#ifndef MYREDIS_NETWORK_SEND_ALL_H_
#define MYREDIS_NETWORK_SEND_ALL_H_
#include <unistd.h>

#include <string_view>

namespace myredis {

ssize_t SendAll(int socket_fd, std::string_view data, int flags);

}  // namespace myredis

#endif  // MYREDIS_NETWORK_SEND_ALL_H_
