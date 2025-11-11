#include "RespValueQueue.h"

#include <cassert>
#include <stdexcept>

RespValueQueue::RespValueQueue() {}

void RespValueQueue::PushString(const std::string& str) {
  buffer_.append(str);

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
  } catch (const std::out_of_range&) {
    // Incomplete input: leave the partial data in buffer_ and wait for more.
    return;
  }
  // let std::invalid_argument propagate to caller
}

bool RespValueQueue::HasValue() const { return !values_.empty(); }

RespValue RespValueQueue::PopValue() {
  assert(HasValue());
  RespValue value = values_.front();
  values_.pop();
  return value;
}
