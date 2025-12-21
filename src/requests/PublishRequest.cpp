#include "PublishRequest.h"

#include <stdexcept>

PublishRequest::PublishRequest(const RespValue& resp_value) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    throw std::invalid_argument("Publish must be an array of values");

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 3)
    throw std::invalid_argument(
        "Publish must have exactly three elements: PUBLISH, channel, and message");

  const RespValue& publish = request_array[0];
  const RespValue& channel = request_array[1];
  const RespValue& message = request_array[2];

  if (!(std::holds_alternative<RespValue::RespBulkString>(publish.getValue()) &&
        std::get<RespValue::RespBulkString>(publish.getValue()) == "PUBLISH"))
    throw std::invalid_argument("Resp value is not a PUBLISH request");

  if (!(std::holds_alternative<RespValue::RespBulkString>(channel.getValue()) &&
        std::holds_alternative<RespValue::RespBulkString>(message.getValue())))
    throw std::invalid_argument("Arguments are not strings");

  const auto& channel_optional_string =
      std::get<RespValue::RespBulkString>(channel.getValue());
  if (!channel_optional_string.has_value() || channel_optional_string->empty())
    throw std::invalid_argument("Channel argument is empty");

  const auto& message_optional_string =
      std::get<RespValue::RespBulkString>(message.getValue());
  if (!message_optional_string.has_value() || message_optional_string->empty())
    throw std::invalid_argument("Message argument is empty");

  channel_ = *channel_optional_string;
  message_ = *message_optional_string;
}

const std::string& PublishRequest::getChannel() const { return channel_; }

const std::string& PublishRequest::getMessage() const { return message_; }

bool PublishRequest::IsRequest(const RespValue& resp_value) {
  if (!std::holds_alternative<RespValue::RespArray>(resp_value.getValue()))
    return false;

  const auto& request_array =
      std::get<RespValue::RespArray>(resp_value.getValue());

  if (request_array.size() != 3) return false;

  const RespValue& publish = request_array[0];
  const RespValue& channel = request_array[1];
  const RespValue& message = request_array[2];

  if (!(std::holds_alternative<RespValue::RespBulkString>(publish.getValue()) &&
        std::get<RespValue::RespBulkString>(publish.getValue()) == "PUBLISH"))
    return false;

  if (!(std::holds_alternative<RespValue::RespBulkString>(channel.getValue()) &&
        std::holds_alternative<RespValue::RespBulkString>(message.getValue())))
    return false;

  const auto& channel_optional_string =
      std::get<RespValue::RespBulkString>(channel.getValue());
  if (!channel_optional_string.has_value() || channel_optional_string->empty())
    return false;

  const auto& message_optional_string =
      std::get<RespValue::RespBulkString>(message.getValue());
  if (!message_optional_string.has_value() || message_optional_string->empty())
    return false;

  return true;
}
