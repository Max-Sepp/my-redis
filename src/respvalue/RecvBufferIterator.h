#ifndef MY_REDIS_RECVBUFFERITERATOR_H
#define MY_REDIS_RECVBUFFERITERATOR_H

#include <cstddef>
#include <iterator>

class RecvBufferIterator {
  int socket_fd;
  static constexpr size_t buffer_size = 1024;
  char buffer[buffer_size];
  size_t buffer_pos;
  size_t buffer_end;
  bool end;

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
  void fill_buffer();
};

#endif  // MY_REDIS_RECVBUFFERITERATOR_H
