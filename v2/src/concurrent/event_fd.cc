#include "event_fd.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace myredis {

EventFd::EventFd() : fd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {}

EventFd::~EventFd() {
  if (fd_ >= 0) close(fd_);
}

void EventFd::Notify() const {
  if (fd_ < 0) return;
  constexpr std::uint64_t one = 1;
  // A counting eventfd write never blocks unless the counter would overflow
  // (2^64 - 1), which we never reach in practice. Retry on EINTR.
  while (write(fd_, &one, sizeof(one)) < 0 && errno == EINTR) {
  }
}

void EventFd::Drain() const {
  if (fd_ < 0) return;
  std::uint64_t value = 0;
  // Non-blocking read clears the counter in one call; loop only to absorb
  // EINTR. EAGAIN means it was already drained.
  while (read(fd_, &value, sizeof(value)) < 0 && errno == EINTR) {
  }
}

}  // namespace myredis
