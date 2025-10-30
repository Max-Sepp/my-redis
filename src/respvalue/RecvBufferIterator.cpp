#include "RecvBufferIterator.h"

#include <sys/socket.h>

#include <stdexcept>

RecvBufferIterator::RecvBufferIterator(RecvBufferIterator&& other) noexcept
    : socket_fd(other.socket_fd),
      buffer(other.buffer),
      pos(other.pos),
      prev_num_chars_read_from_socket(other.prev_num_chars_read_from_socket),
      num_chars_read_from_socket(other.num_chars_read_from_socket),
      end(other.end) {}

RecvBufferIterator& RecvBufferIterator::operator=(
    const RecvBufferIterator& other) {
  if (this == &other) return *this;
  socket_fd = other.socket_fd;
  buffer = other.buffer;
  pos = other.pos;
  prev_num_chars_read_from_socket = other.prev_num_chars_read_from_socket;
  num_chars_read_from_socket = other.num_chars_read_from_socket;
  end = other.end;
  return *this;
}

RecvBufferIterator& RecvBufferIterator::operator=(
    RecvBufferIterator&& other) noexcept {
  if (this == &other) return *this;
  socket_fd = other.socket_fd;
  buffer = other.buffer;
  pos = other.pos;
  prev_num_chars_read_from_socket = other.prev_num_chars_read_from_socket;
  num_chars_read_from_socket = other.num_chars_read_from_socket;
  end = other.end;
  return *this;
}

RecvBufferIterator::RecvBufferIterator(const int socket_fd)
    : socket_fd(socket_fd),
      buffer{},
      pos(0),
      prev_num_chars_read_from_socket(0),
      num_chars_read_from_socket(0),
      end(false) {}

RecvBufferIterator::RecvBufferIterator()
    : socket_fd(-1),
      buffer{},
      pos(0),
      prev_num_chars_read_from_socket(0),
      num_chars_read_from_socket(0),
      end(true) {}

char RecvBufferIterator::operator*() const {
  if (pos >= num_chars_read_from_socket) fill_buffer();
  return buffer[pos - prev_num_chars_read_from_socket];
}

RecvBufferIterator& RecvBufferIterator::operator++() {
  pos++;
  return *this;
}

/**
 * This only compares based on whether the iterator is at the end
 * i.e. this == other iff this and other are both at the end or this and other
 * are both not at the end
 */
bool RecvBufferIterator::operator==(const RecvBufferIterator& other) const {
  if (pos >= num_chars_read_from_socket) fill_buffer();
  return end == other.end;
}

bool RecvBufferIterator::operator!=(const RecvBufferIterator& other) const {
  return !(*this == other);
}
void RecvBufferIterator::fill_buffer() const {
  ssize_t n = recv(socket_fd, buffer.data(), buffer.size(), 0);
  if (n > 0) {
    prev_num_chars_read_from_socket = num_chars_read_from_socket;
    num_chars_read_from_socket += static_cast<size_t>(n);
  } else if (n == 0) {
    end = true;
  } else {
    throw std::runtime_error("recv error");
  }
}
