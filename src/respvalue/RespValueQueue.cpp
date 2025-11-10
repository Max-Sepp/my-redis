#include "RespValueQueue.h"

#include <stdexcept>

RespValueQueue::RespValueQueue() : valid_(false), value_(std::nullopt) {}

bool RespValueQueue::PushString(const std::string& str) {
  const size_t prev_size = buffer_.size();
  buffer_.append(str);

  if (IsValid()) return true;

  try {
    auto [val, pos] = RespValue::FromString(buffer_);
    // Successfully parsed a value that consumed 'pos' chars of buffer_.
    value_ = std::move(val);
    valid_ = true;

    // Remove consumed prefix from buffer_
    if (pos < buffer_.size()) {
      buffer_ = buffer_.substr(pos);
    } else {
      buffer_.clear();
    }

    return true;
  } catch (const std::out_of_range&) {
    // Incomplete input: we consumed all of `str` into buffer but no complete
    // value yet.
    return false;
  }
  // let std::invalid_argument propagate to caller
}

bool RespValueQueue::IsValid() const { return valid_; }

RespValue RespValueQueue::PopValue() {
  if (!valid_ || !value_.has_value()) {
    throw std::runtime_error("No RespValue available");
  }
  valid_ = false;
  RespValue ret = std::move(*value_);
  value_.reset();
  return ret;
}
