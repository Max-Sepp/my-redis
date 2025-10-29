#include "RespValue.h"

#include <sstream>
#include <stdexcept>

#include "RecvBufferIterator.h"

namespace {
template <typename InputIterator>
std::string ReadLine(InputIterator& it, InputIterator& end);
template <typename InputIterator>
RespValue::RespVariant ParseValue(InputIterator& it, InputIterator& end);
template <typename InputIterator>
RespValue::RespSimpleString ParseSimpleString(InputIterator& it,
                                              InputIterator& end);
template <typename InputIterator>
RespValue::RespSimpleError ParseSimpleError(InputIterator& it,
                                            InputIterator& end);
template <typename InputIterator>
RespValue::RespInteger ParseInteger(InputIterator& it, InputIterator& end);
template <typename InputIterator>
RespValue::RespBulkString ParseBulkString(InputIterator& it,
                                          InputIterator& end);
template <typename InputIterator>
RespValue::RespArray ParseArray(InputIterator& it, InputIterator& end);

template <typename InputIterator>
std::string ReadLine(InputIterator& it, InputIterator& end) {
  std::string line;
  while (it != end) {
    const char c = *it;
    ++it;
    if (c == '\r') {
      if (*it == '\n') {
        ++it;
        return line;
      }
      throw std::invalid_argument("Invalid delimiter");
    }
    line += c;
  }
  throw std::out_of_range(
      "Could not find delimiting \\r\\n string in the input string");
}

template <typename InputIterator>
RespValue::RespVariant ParseValue(InputIterator& it, InputIterator& end) {
  if (it == end) {
    throw std::out_of_range("Not enough data to parse value type");
  }
  const char type_char = *it;
  ++it;
  switch (type_char) {
    case '+':
      return ParseSimpleString(it, end);
    case '-':
      return ParseSimpleError(it, end);
    case ':':
      return ParseInteger(it, end);
    case '$':
      return ParseBulkString(it, end);
    case '*':
      return ParseArray(it, end);
    default:
      throw std::invalid_argument("Invalid resp string parsed in");
  }
}

template <typename InputIterator>
RespValue::RespSimpleString ParseSimpleString(InputIterator& it,
                                              InputIterator& end) {
  return ReadLine(it, end);
}

template <typename InputIterator>
RespValue::RespSimpleError ParseSimpleError(InputIterator& it,
                                            InputIterator& end) {
  return RespValue::RespSimpleError{.message = ReadLine(it, end)};
}

template <typename InputIterator>
RespValue::RespInteger ParseInteger(InputIterator& it, InputIterator& end) {
  const std::string integer_string = ReadLine(it, end);
  return stoll(integer_string);
}

template <typename InputIterator>
RespValue::RespBulkString ParseBulkString(InputIterator& it,
                                          InputIterator& end) {
  const std::string length_string = ReadLine(it, end);
  const long long bulk_string_length = stoll(length_string);

  if (bulk_string_length == -1) {
    return std::nullopt;
  }
  if (bulk_string_length < -1) {
    throw std::invalid_argument(
        "Bulk string has a negative length and is not null bulk string");
  }

  std::string bulkString;
  bulkString.reserve(bulk_string_length);
  for (long long i = 0; i < bulk_string_length; ++i) {
    if (it == end) {
      throw std::out_of_range("Incomplete bulk string content");
    }
    bulkString += *it;
    ++it;
  }

  if (it == end || *it != '\r') {
    throw std::invalid_argument("Bulk string not followed by \\r\\n");
  }
  ++it;
  if (it == end || *it != '\n') {
    throw std::invalid_argument("Bulk string not followed by \\r\\n");
  }
  ++it;

  return bulkString;
}

template <typename InputIterator>
RespValue::RespArray ParseArray(InputIterator& it, InputIterator& end) {
  const std::string length_string = ReadLine(it, end);
  const long long array_length = stoll(length_string);

  std::vector<RespValue> output;
  for (size_t i = 0; i < array_length; i++) {
    output.emplace_back(ParseValue(it, end));
  }
  return output;
}
}  // namespace

RespValue::RespValue(RecvBufferIterator begin, RecvBufferIterator end) {
  value = ParseValue(begin, end);
  // We don't check if the whole buffer is consumed, as there might be more
  // commands pipelined.
}

RespValue::RespValue(const std::string& resp_string) {
  auto begin = resp_string.cbegin();
  auto end = resp_string.cend();
  value = ParseValue(begin, end);

  if (begin != end) {
    throw std::invalid_argument("Extra data after valid RESP message");
  }
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
