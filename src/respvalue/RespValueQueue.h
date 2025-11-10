#ifndef MY_REDIS_RESPVALUEQUEUE_H
#define MY_REDIS_RESPVALUEQUEUE_H

#include <optional>
#include <string>

#include "RespValue.h"

class RespValueQueue {
 public:
  RespValueQueue();
  /* Append `str` to the internal buffer and attempt to parse a RespValue.
   * Returns the number of characters from `str` that were consumed.
   *
   * Behavior:
   * - If a full RespValue is parsed, it is stored internally, IsValid() becomes
   *   true, the consumed prefix is removed from the buffer, and PushString()
   *   returns true.
   * - If parsing fails with std::out_of_range, the input is incomplete: the
   *   fragment is appended and the function returns false waiting for more
   *   data.
   * - If parsing fails with std::invalid_argument, the exception is rethrown.
   */
  bool PushString(const std::string &str);

  bool IsValid() const;

  RespValue PopValue();

 private:
  std::string buffer_;
  bool valid_;
  std::optional<RespValue> value_;
};

#endif  // MY_REDIS_RESPVALUEQUEUE_H