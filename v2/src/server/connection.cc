#include "connection.h"

#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <optional>
#include <utility>

namespace myredis {

namespace {
constexpr std::size_t kReadBufSize = 4096;

// recv/send on a non-blocking, level-triggered socket can report these to mean
// "nothing more right now" rather than a real failure. EINTR is grouped here
// because the socket stays readable/writable and epoll will fire again.
bool WouldBlockOrInterrupted(const ssize_t result) {
  return result < 0 &&
         (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
}
}  // namespace

bool Connection::ReadIntoParseQueue() {
  std::array<char, kReadBufSize> buf{};
  ssize_t num_bytes_consumed = recv(fd, buf.data(), buf.size(), MSG_DONTWAIT);
  while (num_bytes_consumed > 0) {
    parse_queue.PushString(
        std::string(buf.data(), static_cast<std::size_t>(num_bytes_consumed)));
    num_bytes_consumed = recv(fd, buf.data(), buf.size(), MSG_DONTWAIT);
  }

  if (num_bytes_consumed == 0)
    return false;  // peer performed an orderly shutdown
  // n < 0: drained (still alive) for would-block/interrupt, otherwise fatal.
  return WouldBlockOrInterrupted(num_bytes_consumed);
}

std::vector<RespValue> Connection::TakeParsedRequests() {
  std::vector<RespValue> requests;
  while (std::optional<RespValue> request = parse_queue.PopValue()) {
    requests.push_back(std::move(*request));
  }
  return requests;
}

bool Connection::WriteOutBuffer() {
  // Write as much of out_buffer as the socket will currently accept, tracking
  // exactly how many bytes were consumed so the remainder can be retried on
  // EPOLLOUT. (SendAll is unsuitable here: it cannot report partial progress
  // when a non-blocking send would block.)
  std::size_t sent = 0;
  bool would_block = false;
  while (sent < out_buffer.size() && !would_block) {
    const ssize_t num_bytes_sent =
        send(fd, out_buffer.data() + sent, out_buffer.size() - sent,
             MSG_NOSIGNAL | MSG_DONTWAIT);
    if (num_bytes_sent > 0) {
      sent += static_cast<std::size_t>(num_bytes_sent);
    } else if (WouldBlockOrInterrupted(num_bytes_sent)) {
      would_block = true;
    } else {
      return false;  // fatal write error
    }
  }

  out_buffer.erase(0, sent);
  return true;
}

}  // namespace myredis
