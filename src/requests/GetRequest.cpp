#include "GetRequest.h"

GetRequest::GetRequest(const RespValue& resp_value) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    throw std::invalid_argument(
        "Get request resp must be an array containing GET and then the <key>");

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 2)
    throw std::invalid_argument(
        "Get request resp must be an array containing GET and then the <key>");

  const RespValue& get = request_array[0];
  const RespValue& key = request_array[1];

  if (!(std::holds_alternative<RespValue::RespBulkString>(get.getValue()) &&
        std::get<RespValue::RespBulkString>(get.getValue()) == "GET"))
    throw std::invalid_argument(
        "Get request resp must be an array containing GET and then the <key>");

  if (!(std::holds_alternative<RespValue::RespBulkString>(key.getValue())))
    throw std::invalid_argument("Key is not a bulk string");

  const auto& key_string = std::get<RespValue::RespBulkString>(key.getValue());

  if (!key_string.has_value() || key_string == "")
    throw std::invalid_argument("Key is not a valid key");

  key_ = key_string.value();
}

const std::string& GetRequest::getKey() const { return key_; }

bool GetRequest::IsRequest(const RespValue& resp_value) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    return false;

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 2) return false;

  const RespValue& get = request_array[0];
  const RespValue& key = request_array[1];

  if (!(std::holds_alternative<RespValue::RespBulkString>(get.getValue()) &&
        std::get<RespValue::RespBulkString>(get.getValue()) == "GET"))
    return false;

  if (!(std::holds_alternative<RespValue::RespBulkString>(key.getValue())))
    return false;

  const auto& key_string = std::get<RespValue::RespBulkString>(key.getValue());
  if (!key_string.has_value() || key_string == "") return false;

  return true;
}