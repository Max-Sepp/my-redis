#ifndef MY_REDIS_RECVBUFFERITERATOR_H
#define MY_REDIS_RECVBUFFERITERATOR_H

#include <array>
#include <cstddef>

class RecvBufferIterator {
 private:
  int socket_fd;
  static constexpr size_t buffer_size = 1024;
  mutable std::array<char, buffer_size> buffer;
  size_t pos;
  mutable size_t prev_num_chars_read_from_socket;
  mutable size_t num_chars_read_from_socket;
  mutable bool end;

 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = char;
  using difference_type = std::ptrdiff_t;
  using pointer = char*;
  using reference = char&;

  RecvBufferIterator(const RecvBufferIterator& other) = default;
  RecvBufferIterator(RecvBufferIterator&& other) noexcept;
  RecvBufferIterator& operator=(const RecvBufferIterator& other);
  RecvBufferIterator& operator=(RecvBufferIterator&& other) noexcept;

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
