#include "RespValues.h"

RespValue NullBulkString() { return RespValue::FromVariant(std::nullopt); }

RespValue BulkString(const std::optional<std::string>& string) {
  return RespValue::FromVariant(string);
}

RespValue Integer(long long num) { return RespValue::FromVariant(num); }

RespValue Error(const std::string& message) {
  return RespValue::FromVariant(RespValue::RespSimpleError{message});
}