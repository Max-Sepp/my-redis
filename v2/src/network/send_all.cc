#include "send_all.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <string_view>

namespace myredis {

ssize_t SendAll(const int socket_fd, std::string_view data, const int flags) {
  const auto total = static_cast<ssize_t>(data.size());
  while (!data.empty()) {
    const ssize_t sent = send(socket_fd, data.data(), data.size(), flags);
    if (sent > 0) {
      data.remove_prefix(static_cast<size_t>(sent));
    } else if (sent == 0) {  // peer closed
      return total - static_cast<ssize_t>(data.size());
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // For non-blocking sockets: caller must wait for writable
      // (select/poll/epoll) then retry
      return -2;                  // indicate would-block to caller
    } else if (errno != EINTR) {  // EINTR: loop round and retry
      return -1;                  // other fatal error
    }
  }
  return total;  // all bytes sent
}

}  // namespace myredis
