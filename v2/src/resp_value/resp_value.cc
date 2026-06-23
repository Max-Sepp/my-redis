#include "resp_value.h"

#include <cassert>
#include <stdexcept>

namespace myredis {

RespValue::RespValue(RespVariant variant) : value_(std::move(variant)) {}

RespValue::RespVariant RespValue::ParseVariant(const std::string& str,
                                               size_t& pos) {
  if (pos >= str.size()) {
    throw std::out_of_range("Unexpected end of input while parsing RESP value");
  }
  switch (str[pos]) {
    case '+':
      return ParseSimpleString(str, pos);
    case '-':
      return ParseSimpleError(str, pos);
    case ':':
      return ParseInteger(str, pos);
    case '$':
      return ParseBulkString(str, pos);
    case '*':
      return ParseArray(str, pos);
    default:
      throw std::invalid_argument("Invalid resp type prefix");
  }
}

RespValue::RespSimpleString RespValue::ParseSimpleString(const std::string& str,
                                                         size_t& pos) {
  assert(str[pos] == '+');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    throw std::out_of_range("Missing CRLF for simple string");
  }
  const std::string simple_string = str.substr(pos + 1, end_pos - pos - 1);
  pos = end_pos + 2;  // skip \r\n
  return simple_string;
}

RespValue::RespSimpleError RespValue::ParseSimpleError(const std::string& str,
                                                       size_t& pos) {
  assert(str[pos] == '-');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    throw std::out_of_range("Missing CRLF for simple error");
  }
  const std::string error_message = str.substr(pos + 1, end_pos - pos - 1);
  pos = end_pos + 2;  // skip \r\n
  return RespSimpleError{.message = error_message};
}

RespValue::RespInteger RespValue::ParseInteger(const std::string& str,
                                               size_t& pos) {
  assert(str[pos] == ':');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    throw std::out_of_range("Missing CRLF for integer");
  }
  const std::string integer_string = str.substr(pos + 1, end_pos - pos - 1);
  const int64_t integer_value = stoll(integer_string);
  pos = end_pos + 2;  // skip \r\n
  return integer_value;
}

RespValue::RespBulkString RespValue::ParseBulkString(const std::string& str,
                                                     size_t& pos) {
  assert(str[pos] == '$');
  const size_t end_of_length = str.find("\r\n", pos + 1);
  if (end_of_length == std::string::npos) {
    throw std::out_of_range("Missing CRLF after bulk-string length");
  }
  const std::string length_string = str.substr(pos + 1, end_of_length - pos - 1);
  const long long bulk_string_length = stoll(length_string);
  pos = end_of_length + 2;
  if (bulk_string_length == -1) {
    return std::nullopt;
  }
  if (bulk_string_length <= -2) {
    throw std::invalid_argument(
        "Bulk string has a negative length and is not null bulk string");
  }
  // Ensure there's enough data for the bulk string content plus trailing CRLF.
  if (bulk_string_length < 0 ||
      (pos + static_cast<size_t>(bulk_string_length) + 2) > str.size()) {
    throw std::out_of_range("Bulk string payload truncated or missing CRLF");
  }
  // Verify terminating CRLF after payload.
  if (str[pos + bulk_string_length] != '\r' ||
      str[pos + bulk_string_length + 1] != '\n') {
    throw std::out_of_range("Bulk string missing terminating CRLF");
  }
  std::string bulk_string =
      str.substr(pos, static_cast<size_t>(bulk_string_length));
  pos += static_cast<size_t>(bulk_string_length) + 2;
  return bulk_string;
}

RespValue::RespArray RespValue::ParseArray(const std::string& str,
                                           size_t& pos) {
  assert(str[pos] == '*');
  const size_t end_of_length = str.find("\r\n", pos + 1);
  if (end_of_length == std::string::npos) {
    throw std::out_of_range("Missing CRLF after array length");
  }
  const std::string length_string = str.substr(pos + 1, end_of_length - pos - 1);
  const long long array_length = stoll(length_string);
  pos = end_of_length + 2;
  if (array_length < 0) {
    throw std::invalid_argument("Negative array length not allowed");
  }
  std::vector<RespValue> output;
  for (long long i = 0; i < array_length; ++i) {
    if (pos >= str.size()) {
      throw std::out_of_range("Array element missing/truncated");
    }
    output.push_back(RespValue(ParseVariant(str, pos)));
  }
  return output;
}

std::string RespValue::Serialize() const {
  return std::visit(
      []<typename RespVariant>(const RespVariant& val) -> std::string {
        using T = std::decay_t<RespVariant>;

        if constexpr (std::is_same_v<T, RespSimpleString>) {
          return "+" + val + "\r\n";
        } else if constexpr (std::is_same_v<T, RespSimpleError>) {
          return "-" + val.message + "\r\n";
        } else if constexpr (std::is_same_v<T, RespInteger>) {
          return ":" + std::to_string(val) + "\r\n";
        } else if constexpr (std::is_same_v<T, RespBulkString>) {
          if (!val.has_value()) {
            return "$-1\r\n";
          }
          const std::string& str = val.value();
          return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
        } else if constexpr (std::is_same_v<T, RespArray>) {
          std::string result = "*" + std::to_string(val.size()) + "\r\n";
          for (const auto& element : val) {
            result += element.Serialize();
          }
          return result;
        }
        throw std::invalid_argument("Resp Value variant not a valid variant");
      },
      value_);
}

const RespValue::RespVariant& RespValue::GetValue() const { return value_; }

std::string RespValue::Show() const {
  return std::visit(
      []<typename RespVariant>(const RespVariant& val) -> std::string {
        using T = std::decay_t<RespVariant>;

        if constexpr (std::is_same_v<T, RespSimpleString>) {
          return "\"" + val + "\"";
        } else if constexpr (std::is_same_v<T, RespSimpleError>) {
          return "error: " + val.message;
        } else if constexpr (std::is_same_v<T, RespInteger>) {
          return std::to_string(val);
        } else if constexpr (std::is_same_v<T, RespBulkString>) {
          if (!val.has_value()) {
            return "(NIL)";
          }
          const std::string& str = val.value();
          return "\"" + str + "\"";
        } else if constexpr (std::is_same_v<T, RespArray>) {
          std::string result;
          for (size_t i = 0; i < val.size(); i++) {
            if (i != 0) result += "\n";
            result += std::to_string(i + 1) + ") " + val[i].Show();
          }
          return result;
        }
        throw std::invalid_argument("Resp Value variant not a valid variant");
      },
      value_);
}

std::pair<RespValue, size_t> RespValue::FromString(const std::string& str) {
  size_t pos = 0;
  return std::make_pair(RespValue(ParseVariant(str, pos)), pos);
}

RespValue RespValue::FromVariant(const RespVariant& variant) {
  return RespValue(variant);
}

}  // namespace myredis
