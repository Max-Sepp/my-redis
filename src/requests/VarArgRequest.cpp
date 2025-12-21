#include "VarArgRequest.h"

#include <stdexcept>

VarArgRequest::VarArgRequest(const RespValue& resp_value,
                             std::string request_type)
    : request_type_(std::move(request_type)) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    throw std::invalid_argument(request_type +
                                " request resp must be an array containing " +
                                request_type + " and then the arguments");

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() <= 1)
    throw std::invalid_argument(request_type +
                                " request resp must be an array containing " +
                                request_type + " and then the arguments");

  if (const RespValue& get = request_array[0];
      !(std::holds_alternative<RespValue::RespBulkString>(get.getValue()) &&
        std::get<RespValue::RespBulkString>(get.getValue()) == request_type_))
    throw std::invalid_argument(request_type +
                                " request resp must be an array containing " +
                                request_type + " and then the arguments");

  for (int i = 1; i < request_array.size(); i++) {
    const RespValue& value = request_array[i];

    if (!std::holds_alternative<RespValue::RespBulkString>(value.getValue()))
      throw std::invalid_argument("Element is not a bulk string");

    const auto& value_string =
        std::get<RespValue::RespBulkString>(value.getValue());

    if (!value_string.has_value() || value_string->empty())
      throw std::invalid_argument("Key is not a valid key");

    args_.push_back(value_string.value());
  }
}

VarArgRequest::VarArgRequest(std::string request_type)
    : request_type_(std::move(request_type)) {}

int VarArgRequest::numArgs() const { return static_cast<int>(args_.size()); }

const std::string& VarArgRequest::getArg(const int index) const {
  if (index < 0 || numArgs() <= index)
    throw std::out_of_range("Index out of range");
  return args_[index];
}

bool VarArgRequest::IsRequest(const RespValue& resp_value) const {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    return false;

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() <= 1) return false;

  if (const RespValue& get = request_array[0];
      !(std::holds_alternative<RespValue::RespBulkString>(get.getValue()) &&
        std::get<RespValue::RespBulkString>(get.getValue()) == request_type_))
    return false;

  return true;
}
