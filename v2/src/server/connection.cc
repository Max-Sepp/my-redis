#include "connection.h"

#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <optional>
#include <stdexcept>
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

Connection::ReadResult Connection::Read() {
  std::array<char, kReadBufSize> buf{};
  ssize_t num_bytes_consumed = recv(fd_, buf.data(), buf.size(), MSG_DONTWAIT);
  while (num_bytes_consumed > 0) {
    parse_queue_.PushString(
        std::string(buf.data(), static_cast<std::size_t>(num_bytes_consumed)));
    num_bytes_consumed = recv(fd_, buf.data(), buf.size(), MSG_DONTWAIT);
  }

  // A zero return is an orderly peer shutdown; a negative one is fine only for
  // would-block/interrupt, otherwise it is a fatal error.
  ReadResult result{.requests = {},
                    .keep_open = num_bytes_consumed != 0 &&
                                 WouldBlockOrInterrupted(num_bytes_consumed)};

  try {
    while (std::optional<RespValue> request = parse_queue_.PopValue()) {
      result.requests.push_back(std::move(*request));
    }
  } catch (const std::invalid_argument&) {
    // Malformed RESP framing: drop the connection.
    result.requests.clear();
    result.keep_open = false;
  }
  return result;
}

bool Connection::Send(const std::string_view bytes) {
  out_buffer_.append(bytes);
  return Flush();
}

bool Connection::Flush() {
  // Write as much of out_buffer_ as the socket will currently accept, tracking
  // exactly how many bytes were consumed so the remainder can be retried on
  // EPOLLOUT. (SendAll is unsuitable here: it cannot report partial progress
  // when a non-blocking send would block.)
  std::size_t sent = 0;
  bool would_block = false;
  while (sent < out_buffer_.size() && !would_block) {
    const ssize_t num_bytes_sent =
        send(fd_, out_buffer_.data() + sent, out_buffer_.size() - sent,
             MSG_NOSIGNAL | MSG_DONTWAIT);
    if (num_bytes_sent > 0) {
      sent += static_cast<std::size_t>(num_bytes_sent);
    } else if (WouldBlockOrInterrupted(num_bytes_sent)) {
      would_block = true;
    } else {
      return false;  // fatal write error
    }
  }

  out_buffer_.erase(0, sent);
  return true;
}

}  // namespace myredis
