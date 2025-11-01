#include "OneArgRequest.h"

#include <stdexcept>

OneArgRequest::OneArgRequest(const RespValue& resp_value,
                             const std::string& request_type) {
  request_type_ = request_type;

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
        std::get<RespValue::RespBulkString>(get.getValue()) == request_type_))
    throw std::invalid_argument(
        "Get request resp must be an array containing GET and then the <key>");

  if (!(std::holds_alternative<RespValue::RespBulkString>(key.getValue())))
    throw std::invalid_argument("Key is not a bulk string");

  const auto& first_arg_string =
      std::get<RespValue::RespBulkString>(key.getValue());

  if (!first_arg_string.has_value() || first_arg_string == "")
    throw std::invalid_argument("Key is not a valid key");

  first_arg_ = first_arg_string.value();
}

OneArgRequest::OneArgRequest(const std::string& request_type)
    : first_arg_(""), request_type_(request_type) {}

const std::string& OneArgRequest::getFirstArg() const {
  if (first_arg_ != "") return first_arg_;
  throw std::runtime_error("Cannot get argument of IsRequest checker");
}

bool OneArgRequest::IsRequest(const RespValue& resp_value) const {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    return false;

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 2) return false;

  const RespValue& get = request_array[0];
  const RespValue& key = request_array[1];

  if (!(std::holds_alternative<RespValue::RespBulkString>(get.getValue()) &&
        std::get<RespValue::RespBulkString>(get.getValue()) ==
            this->request_type_))
    return false;

  if (!(std::holds_alternative<RespValue::RespBulkString>(key.getValue())))
    return false;

  const auto& key_string = std::get<RespValue::RespBulkString>(key.getValue());
  if (!key_string.has_value() || key_string == "") return false;

  return true;
}
