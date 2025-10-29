#include "SetRequest.h"

#include <stdexcept>

SetRequest::SetRequest(const RespValue& resp_value) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    throw std::invalid_argument(
        "Get request resp must be an array containing SET and then the <key>");

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 3)
    throw std::invalid_argument(
        "Get request resp must be an array containing SET and then the <key>");

  const RespValue& set = request_array[0];
  const RespValue& key = request_array[1];
  const RespValue& value = request_array[2];

  if (!(std::holds_alternative<RespValue::RespBulkString>(set.getValue()) &&
        std::get<RespValue::RespBulkString>(set.getValue()) == "SET"))
    throw std::invalid_argument(
        "Get request resp must be an array containing SET and then the <key>");

  if (!(std::holds_alternative<RespValue::RespBulkString>(key.getValue()) &&
        std::holds_alternative<RespValue::RespBulkString>(value.getValue())))
    throw std::invalid_argument("Key is not a bulk string");

  const auto& key_string = std::get<RespValue::RespBulkString>(key.getValue());

  if (!key_string.has_value() || key_string == "")
    throw std::invalid_argument("Key is not a valid key");

  key_ = key_string.value();
  // RespValue::RespBulkString == std::optional<std::string>
  value_ = std::get<RespValue::RespBulkString>(value.getValue());
}

const std::string& SetRequest::getKey() const { return this->key_; }

const std::optional<std::string>& SetRequest::getValue() const {
  return this->value_;
}

bool SetRequest::IsRequest(const RespValue& resp_value) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    return false;

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 3) return false;

  const RespValue& set = request_array[0];
  const RespValue& key = request_array[1];
  const RespValue& value = request_array[2];

  if (!(std::holds_alternative<RespValue::RespBulkString>(set.getValue()) &&
        std::get<RespValue::RespBulkString>(set.getValue()) == "SET"))
    return false;

  if (!(std::holds_alternative<RespValue::RespBulkString>(key.getValue()) &&
        std::holds_alternative<RespValue::RespBulkString>(value.getValue())))
    return false;

  const auto& key_optional_string =
      std::get<RespValue::RespBulkString>(key.getValue());
  if (!key_optional_string.has_value() || key_optional_string == "")
    return false;

  return true;
}