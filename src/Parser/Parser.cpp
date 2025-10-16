#include "Parser.h"

#include <cassert>
#include <sstream>
#include <stdexcept>

RespValue::RespValue(const std::string& respString) {
  size_t pos = 0;
  value = parseValue(respString, pos);

  // Ensure we've consumed the entire string
  if (pos != respString.length()) {
    throw std::invalid_argument("Extra data after valid RESP message");
  }
}

RespValue::RespVariant RespValue::parseValue(const std::string& str,
                                             size_t& pos) {
  switch (str[pos]) {
    case '+':
      return parseSimpleString(str, pos);
    case '-':
      return parseSimpleError(str, pos);
    case ':':
      return parseInteger(str, pos);
    case '$':
      return parseBulkString(str, pos);
    case '*':
      return parseArray(str, pos);
    default:
      throw std::invalid_argument("Invalid resp string parsed in");
  }
}

RespValue::RespSimpleString RespValue::parseSimpleString(const std::string& str,
                                                         size_t& pos) {
  assert(str[pos] == '+');
  const size_t endPos = str.find("\r\n", pos + 1);
  if (endPos == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string simpleString = str.substr(pos + 1, endPos - pos - 1);
  pos = endPos + 2;  // skip \r\n
  return simpleString;
}

RespValue::RespSimpleError RespValue::parseSimpleError(const std::string& str,
                                                       size_t& pos) {
  assert(str[pos] == '-');
  const size_t endPos = str.find("\r\n", pos + 1);
  if (endPos == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string errorMessage = str.substr(pos + 1, endPos - pos - 1);
  pos = endPos + 2;  // skip \r\n
  return RespSimpleError{.message = errorMessage};
}

RespValue::RespInteger RespValue::parseInteger(const std::string& str,
                                               size_t& pos) {
  assert(str[pos] == ':');
  const size_t endPos = str.find("\r\n", pos + 1);
  if (endPos == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string integerString = str.substr(pos + 1, endPos - pos - 1);
  const int64_t integerValue = stoll(integerString);
  pos = endPos + 2;  // skip \r\n
  return integerValue;
}

RespValue::RespBulkString RespValue::parseBulkString(const std::string& str,
                                                     size_t& pos) {
  assert(str[pos] == '$');
  const size_t endOfLength = str.find("\r\n", pos + 1);
  if (endOfLength == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string lengthString = str.substr(pos + 1, endOfLength - pos - 1);
  const long long bulkStringLength = stoll(lengthString);
  pos = endOfLength + 2;
  if (bulkStringLength == -1) {
    return std::nullopt;
  }
  if (bulkStringLength <= -2) {
    throw std::invalid_argument(
        "Bulk string has a negative length and is not null bulk string");
  }
  std::string bulkString = str.substr(pos, bulkStringLength);
  pos += bulkStringLength + 2;
  return bulkString;
}

RespValue::RespArray RespValue::parseArray(const std::string& str,
                                           size_t& pos) {
  assert(str[pos] == '*');
  const size_t endOfLength = str.find("\r\n", pos + 1);
  if (endOfLength == std::string::npos) {
    throw std::invalid_argument(
        "Could not find delimiting \\r\\n string in the input string");
  }
  const std::string lengthString = str.substr(pos + 1, endOfLength - pos - 1);
  const long long arrayLength = stoll(lengthString);
  pos = endOfLength + 2;
  std::vector<RespValue> output;
  for (size_t i = 0; i < arrayLength; i++) {
    output.emplace_back(parseValue(str, pos));
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