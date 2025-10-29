#ifndef MY_REDIS_RECVBUFFERITERATOR_H
#define MY_REDIS_RECVBUFFERITERATOR_H

#include <array>
#include <cstddef>

class RecvBufferIterator {
  const int socket_fd;
  static constexpr size_t buffer_size = 1024;
  mutable std::array<char, buffer_size> buffer;
  mutable size_t buffer_pos;
  mutable size_t buffer_end;
  mutable bool end;

 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = char;
  using difference_type = std::ptrdiff_t;
  using pointer = char*;
  using reference = char&;

  explicit RecvBufferIterator(int socket_fd);
  RecvBufferIterator();

  char operator*() const;
  RecvBufferIterator& operator++();
  bool operator==(const RecvBufferIterator& other) const;
  bool operator!=(const RecvBufferIterator& other) const;

 private:
  void fill_buffer() const;
};

#endif  // MY_REDIS_RECVBUFFERITERATOR_H
