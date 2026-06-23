#include "send_all.h"

#include <sys/socket.h>

#include <cerrno>
#include <cstdint>

namespace myredis {

ssize_t SendAll(const int fd, const void *buf, const size_t len,
                const int flags) {
  auto p = static_cast<const std::uint8_t *>(buf);
  size_t remaining = len;
  while (remaining > 0) {
    const ssize_t n = send(fd, p, remaining, flags);
    if (n > 0) {
      p += n;
      remaining -= n;
      continue;
    }
    if (n == 0) return len - remaining;  // peer closed
    if (n == -1) {
      if (errno == EINTR) continue;  // retry
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // For non-blocking sockets: caller must wait for writable
        // (select/poll/epoll) then retry
        return -2;  // indicate would-block to caller
      }
      return -1;  // other fatal error
    }
  }
  return len;  // all bytes sent
}

}  // namespace myredis
