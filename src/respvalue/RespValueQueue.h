#ifndef MY_REDIS_RESPVALUEQUEUE_H
#define MY_REDIS_RESPVALUEQUEUE_H

#include <optional>
#include <queue>
#include <string>

#include "RespValue.h"

class RespValueQueue {
 public:
  RespValueQueue();
  /* Append `str` to the internal buffer and attempt to parse zero or more
   * RespValue objects from the buffer. Parsed values are enqueued internally.
   *
   * Behavior:
   * - The function appends all of `str` to the internal buffer_ and then
   *   repeatedly attempts to parse RespValue objects from the buffer.
   * - For each successfully parsed value, the value is moved into the internal
   *   queue_ and the consumed prefix is removed from buffer_.
   * - If parsing encounters incomplete input, RespValue::FromString will throw
   *   std::out_of_range; this function catches that and leaves the (partial)
   *   data in buffer_ waiting for more input.
   * - If parsing fails with std::invalid_argument, that exception is not
   *   caught here and will propagate to the caller.
   */
  void PushString(const std::string& str);

  // Returns true if there is at least one parsed RespValue available to pop.
  bool HasValue() const;

  // Removes and returns the next parsed RespValue. Caller must ensure HasValue()
  // is true before calling.
  RespValue PopValue();

 private:
  std::string buffer_;
  std::queue<RespValue> values_;
};

#endif  // MY_REDIS_RESPVALUEQUEUE_H