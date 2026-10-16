#include "resp_values.h"

#include <optional>
#include <string>
#include <vector>

#include "resp_value/resp_value.h"

namespace myredis {

RespValue NullBulkString() { return RespValue::FromVariant(std::nullopt); }

RespValue SimpleString(const std::string& string) {
  return RespValue::FromVariant(RespValue::RespSimpleString{string});
}

RespValue BulkString(const std::optional<std::string>& string) {
  return RespValue::FromVariant(string);
}

RespValue Integer(long long num) { return RespValue::FromVariant(num); }

RespValue Error(const std::string& message) {
  return RespValue::FromVariant(RespValue::RespSimpleError{message});
}

RespValue Array(const std::vector<RespValue>& resp_values) {
  return RespValue::FromVariant(resp_values);
}

}  // namespace myredis
