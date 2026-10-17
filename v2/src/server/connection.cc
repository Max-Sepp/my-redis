#include "connection.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "resp_value/resp_value.h"

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
  // Queued bytes must reach the socket first to keep responses in order.
  if (HasPendingWrites()) {
    out_buffer_.append(bytes);
    return Flush();
  }
  return SendAndBufferRemainder(bytes);
}

bool Connection::Flush() {
  const std::optional<std::size_t> sent =
      SendUntilWouldBlock(std::string_view(out_buffer_).substr(out_offset_));
  if (!sent) return false;
  out_offset_ += *sent;
  if (out_offset_ == out_buffer_.size()) {
    out_buffer_.clear();
    out_offset_ = 0;
  } else if (out_offset_ > out_buffer_.size() / 2) {
    // Compact only once the sent prefix outweighs the unsent tail, so each
    // byte moved is paid for by a byte already sent: O(1) amortised per byte.
    out_buffer_.erase(0, out_offset_);
    out_offset_ = 0;
  }
  return true;
}

bool Connection::SendAndBufferRemainder(const std::string_view bytes) {
  const std::optional<std::size_t> sent = SendUntilWouldBlock(bytes);
  if (!sent) return false;
  out_buffer_.append(bytes.substr(*sent));
  return true;
}

std::optional<std::size_t> Connection::SendUntilWouldBlock(
    const std::string_view bytes) const {
  // SendAll is unsuitable here: it cannot report partial progress when a
  // non-blocking send would block.
  std::size_t sent = 0;
  bool would_block = false;
  while (sent < bytes.size() && !would_block) {
    const ssize_t num_bytes_sent =
        send(fd_, bytes.data() + sent, bytes.size() - sent,
             MSG_NOSIGNAL | MSG_DONTWAIT);
    if (num_bytes_sent > 0) {
      sent += static_cast<std::size_t>(num_bytes_sent);
    } else if (WouldBlockOrInterrupted(num_bytes_sent)) {
      would_block = true;
    } else {
      return std::nullopt;  // fatal write error
    }
  }
  return sent;
}

}  // namespace myredis
