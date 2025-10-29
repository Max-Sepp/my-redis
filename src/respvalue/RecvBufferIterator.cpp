#include "RecvBufferIterator.h"

#include <sys/socket.h>

#include <stdexcept>

RecvBufferIterator::RecvBufferIterator(const int socket_fd)
    : socket_fd(socket_fd),
      buffer{},
      buffer_pos(0),
      buffer_end(0),
      end(false) {}

RecvBufferIterator::RecvBufferIterator()
    : socket_fd(-1), buffer{}, buffer_pos(0), buffer_end(0), end(true) {}

char RecvBufferIterator::operator*() const {
  if (buffer_pos >= buffer_end) {
    fill_buffer();
  }
  return buffer[buffer_pos];
}

RecvBufferIterator& RecvBufferIterator::operator++() {
  buffer_pos++;
  return *this;
}

bool RecvBufferIterator::operator==(const RecvBufferIterator& other) const {
  if (buffer_pos >= buffer_end) {
    fill_buffer();
  }
  return end == other.end && (end || socket_fd == other.socket_fd);
}

bool RecvBufferIterator::operator!=(const RecvBufferIterator& other) const {
  return !(*this == other);
}
void RecvBufferIterator::fill_buffer() const {
  ssize_t n = recv(socket_fd, buffer.data(), buffer_size, 0);
  if (n > 0) {
    buffer_pos = 0;
    buffer_end = static_cast<size_t>(n);
  } else if (n == 0) {
    end = true;
  } else {
    throw std::runtime_error("recv error");
  }
}
