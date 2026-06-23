#ifndef MYREDIS_NETWORK_SEND_ALL_H_
#define MYREDIS_NETWORK_SEND_ALL_H_
#include <unistd.h>

namespace myredis {

ssize_t SendAll(int fd, const void *buf, size_t len, int flags);

}  // namespace myredis

#endif  // MYREDIS_NETWORK_SEND_ALL_H_
