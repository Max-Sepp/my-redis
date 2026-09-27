#include "resp_value_queue.h"

#include <cassert>

namespace myredis {

void RespValueQueue::PushString(const std::string& str) { buffer_.append(str); }

std::optional<RespValue> RespValueQueue::PopValue() {
  // Try process anything on the string buffer.
  while (true) {
    std::optional<std::pair<RespValue, size_t>> result =
        RespValue::FromString(buffer_);
    if (!result) break;

    auto [val, pos] = *result;

    // Successfully parsed a value that consumed 'pos' chars of buffer_.
    values_.push(std::move(val));

    // Remove consumed prefix from buffer_
    if (pos < buffer_.size()) {
      buffer_ = buffer_.substr(pos);
    } else {
      buffer_.clear();
    }
  }

  if (values_.empty()) return std::nullopt;

  RespValue value = values_.front();
  values_.pop();
  return value;
}

}  // namespace myredis
