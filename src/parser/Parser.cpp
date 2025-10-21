#include "Parser.h"

#include <cassert>
#include <sstream>
#include <stdexcept>

RespValue::RespValue(const std::string& resp_string) {
  size_t pos = 0;
  value = ParseValue(resp_string, pos);

  // Ensure we've consumed the entire string
  if (pos != resp_string.length()) {
    throw std::invalid_argument("Extra data after valid RESP message");
  }
}

RespValue::RespVariant RespValue::ParseValue(const std::string& str,
                                             size_t& pos) {
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
      throw std::invalid_argument("Invalid resp string parsed in");
  }
}

RespValue::RespSimpleString RespValue::ParseSimpleString(const std::string& str,
                                                         size_t& pos) {
  assert(str[pos] == '+');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
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
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
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
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string integer_string = str.substr(pos + 1, end_pos - pos - 1);
  pos = end_pos + 2;  // skip \r\n
  return stoll(integer_string);
}

RespValue::RespBulkString RespValue::ParseBulkString(const std::string& str,
                                                     size_t& pos) {
  assert(str[pos] == '$');
  const size_t end_of_length = str.find("\r\n", pos + 1);
  if (end_of_length == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string length_string =
      str.substr(pos + 1, end_of_length - pos - 1);
  const long long bulk_string_length = stoll(length_string);
  pos = end_of_length + 2;
  if (bulk_string_length == -1) {
    return std::nullopt;
  }
  if (bulk_string_length <= -2) {
    throw std::invalid_argument(
        "Bulk string has a negative length and is not null bulk string");
  }
  std::string bulkString = str.substr(pos, bulk_string_length);
  pos += bulk_string_length + 2;
  return bulkString;
}

RespValue::RespArray RespValue::ParseArray(const std::string& str,
                                           size_t& pos) {
  assert(str[pos] == '*');
  const size_t end_of_length = str.find("\r\n", pos + 1);
  if (end_of_length == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string length_string =
      str.substr(pos + 1, end_of_length - pos - 1);
  const long long array_length = stoll(length_string);
  pos = end_of_length + 2;
  std::vector<RespValue> output;
  for (size_t i = 0; i < array_length; i++) {
    output.emplace_back(ParseValue(str, pos));
  }
  return output;
}

std::string RespValue::serialize() const {
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
            result += element.serialize();
          }
          return result;
        }
        throw std::invalid_argument("Resp Value variant not a valid variant");
      },
      value);
}

const RespValue::RespVariant& RespValue::getValue() const { return value; }
