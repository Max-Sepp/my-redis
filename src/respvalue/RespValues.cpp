#include "RespValues.h"

RespValue NullBulkString() { return RespValue(std::nullopt); }
RespValue BulkString(const std::optional<std::string>& string) {
  return RespValue(string);
}

RespValue Error(const std::string& message) {
  return RespValue(RespValue::RespSimpleError{message});
}