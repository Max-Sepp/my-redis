#include "resp_value_queue.h"

#include <cassert>
#include <stdexcept>

namespace myredis {

void RespValueQueue::PushString(const std::string& str) { buffer_.append(str); }

std::optional<RespValue> RespValueQueue::PopValue() {
  // Try process anything on the string buffer.
  try {
    while (true) {
      auto [val, pos] = RespValue::FromString(buffer_);
      // Successfully parsed a value that consumed 'pos' chars of buffer_.
      values_.push(std::move(val));

      // Remove consumed prefix from buffer_
      if (pos < buffer_.size()) {
        buffer_ = buffer_.substr(pos);
      } else {
        buffer_.clear();
      }
    }
  } catch (const std::out_of_range&) {  // NOLINT(*-empty-catch)
    // Incomplete input: leave the partial data in buffer_ and wait for more.
  }
  // let std::invalid_argument propagate to caller

  if (values_.empty()) return std::nullopt;

  RespValue value = values_.front();
  values_.pop();
  return value;
}

}  // namespace myredis
