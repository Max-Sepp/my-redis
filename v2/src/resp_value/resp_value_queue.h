#ifndef MYREDIS_RESP_VALUE_RESP_VALUE_QUEUE_H_
#define MYREDIS_RESP_VALUE_RESP_VALUE_QUEUE_H_

#include <optional>
#include <queue>
#include <string>

#include "resp_value.h"

namespace myredis {

class RespValueQueue {
 public:
  // Appends `str` to the internal buffer. Parsing happens lazily in PopValue,
  // so this never throws.
  void PushString(const std::string& str);

  /* Parses as many RespValue objects as the buffer allows, then removes and
   * returns the next one (std::nullopt if none are available).
   *
   * Behavior:
   * - Repeatedly attempts to parse RespValue objects from the front of the
   *   buffer. Each successfully parsed value is moved into the internal queue_
   *   and the consumed prefix is removed from the buffer.
   * - If parsing encounters incomplete input, RespValue::FromString throws
   *   std::out_of_range; this is caught and the (partial) data is left in the
   *   buffer waiting for more input.
   * - If parsing fails with std::invalid_argument (malformed framing), that
   *   exception is not caught here and propagates to the caller.
   */
  std::optional<RespValue> PopValue();

 private:
  std::string buffer_;
  std::queue<RespValue> values_;
};

}  // namespace myredis

#endif  // MYREDIS_RESP_VALUE_RESP_VALUE_QUEUE_H_
