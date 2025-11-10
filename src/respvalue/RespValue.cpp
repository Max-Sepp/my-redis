#include "RespValue.h"

#include <cassert>
#include <sstream>
#include <stdexcept>

RespValue::RespValue(RespVariant variant) : value_(std::move(variant)) {}

RespValue::RespVariant RespValue::parseVariant(const std::string& str,
                                               size_t& pos) {
  if (pos >= str.size()) {
    throw std::out_of_range("Unexpected end of input while parsing RESP value");
  }
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
      throw std::invalid_argument("Invalid resp type prefix");
  }
}

RespValue::RespSimpleString RespValue::parseSimpleString(const std::string& str,
                                                         size_t& pos) {
  assert(str[pos] == '+');
  const size_t endPos = str.find("\r\n", pos + 1);
  if (endPos == std::string::npos) {
    throw std::out_of_range("Missing CRLF for simple string");
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
    throw std::out_of_range("Missing CRLF for simple error");
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
    throw std::out_of_range("Missing CRLF for integer");
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
    throw std::out_of_range("Missing CRLF after bulk-string length");
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
  // Ensure there's enough data for the bulk string content plus trailing CRLF.
  if (bulkStringLength < 0 ||
      (static_cast<size_t>(pos) + static_cast<size_t>(bulkStringLength) + 2) >
          str.size()) {
    throw std::out_of_range("Bulk string payload truncated or missing CRLF");
  }
  // Verify terminating CRLF after payload.
  if (str[pos + bulkStringLength] != '\r' ||
      str[pos + bulkStringLength + 1] != '\n') {
    throw std::out_of_range("Bulk string missing terminating CRLF");
  }
  std::string bulkString = str.substr(pos, static_cast<size_t>(bulkStringLength));
  pos += static_cast<size_t>(bulkStringLength) + 2;
  return bulkString;
}

RespValue::RespArray RespValue::parseArray(const std::string& str,
                                           size_t& pos) {
  assert(str[pos] == '*');
  const size_t endOfLength = str.find("\r\n", pos + 1);
  if (endOfLength == std::string::npos) {
    throw std::out_of_range("Missing CRLF after array length");
  }
  const std::string lengthString = str.substr(pos + 1, endOfLength - pos - 1);
  const long long arrayLength = stoll(lengthString);
  pos = endOfLength + 2;
  if (arrayLength < 0) {
    throw std::invalid_argument("Negative array length not allowed");
  }
  std::vector<RespValue> output;
  for (long long i = 0; i < arrayLength; ++i) {
    if (pos >= str.size()) {
      throw std::out_of_range("Array element missing/truncated");
    }
    output.push_back(RespValue(parseVariant(str, pos)));
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
      value_);
}

const RespValue::RespVariant& RespValue::getValue() const { return value_; }

std::pair<RespValue, size_t> RespValue::FromString(const std::string& str) {
  size_t pos = 0;
  return std::make_pair(RespValue(parseVariant(str, pos)), pos);
}

RespValue RespValue::FromVariant(const RespVariant& variant) {
  return RespValue(variant);
}
