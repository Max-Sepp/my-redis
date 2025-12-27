#ifndef MY_REDIS_SENDALL_H
#define MY_REDIS_SENDALL_H
#include <unistd.h>

ssize_t SendAll(int fd, const void *buf, size_t len, int flags);

#endif  // MY_REDIS_SENDALL_H
