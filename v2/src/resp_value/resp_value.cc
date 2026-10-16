#include "resp_value.h"

#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace myredis {

namespace {

struct StringSink : ByteSink {
  void Append(const std::string_view piece) override { bytes.append(piece); }
  std::string bytes;
};

// The RESP type bytes that are followed by a decimal and CRLF.
enum class LinePrefix : char {
  kInteger = ':',
  kBulkLength = '$',
  kArrayLength = '*',
};

// A sign plus digits10 + 1 digits covers every long long.
constexpr std::size_t kMaxDecimalLength =
    std::numeric_limits<long long>::digits10 + 2;

// Appends `<prefix><number>\r\n` without allocating.
void AppendLine(ByteSink& sink, const LinePrefix prefix,
                const long long number) {
  std::array<char, 1 + kMaxDecimalLength + 2> buf{};
  buf[0] = static_cast<char>(prefix);
  const auto [end, ec] =
      std::to_chars(buf.data() + 1, buf.data() + buf.size() - 2, number);
  assert(ec == std::errc());
  end[0] = '\r';
  end[1] = '\n';
  sink.Append({buf.data(), static_cast<std::size_t>(end + 2 - buf.data())});
}

// Wraps a parsed alternative in a RespVariant, selecting the alternative
// explicitly. Returning the optional directly is wrong for
// std::optional<std::string>: it is itself RespBulkString, so the implicit
// conversion would store the whole optional as a bulk string.
template <typename T>
std::optional<RespValue::RespVariant> ToVariant(std::optional<T> value) {
  if (!value) {
    return std::nullopt;
  }
  return RespValue::RespVariant(std::in_place_type<T>, std::move(*value));
}

// Parses str[begin, end) as a signed decimal, optionally prefixed with '+'.
// Uses std::from_chars rather than std::stoll so that overflow is reported as
// std::invalid_argument (a protocol error) instead of std::out_of_range, and
// so that trailing garbage is rejected rather than silently ignored.
long long ParseDecimal(const std::string& str, size_t begin, size_t end) {
  if (begin < end && str[begin] == '+') {
    ++begin;
  }
  const char* first = str.data() + begin;
  const char* last = str.data() + end;
  long long value = 0;
  const auto [parse_end, error_code] = std::from_chars(first, last, value);
  if (error_code != std::errc() || parse_end != last) {
    throw std::invalid_argument("Invalid RESP number");
  }
  return value;
}

}  // namespace

RespValue::RespValue(RespVariant variant) : value_(std::move(variant)) {}

std::optional<RespValue::RespVariant> RespValue::ParseVariant(
    const std::string& str, size_t& pos) {
  if (pos >= str.size()) {
    return std::nullopt;  // End of input
  }
  switch (str[pos]) {
    case '+':
      return ToVariant(ParseSimpleString(str, pos));
    case '-':
      return ToVariant(ParseSimpleError(str, pos));
    case ':':
      return ToVariant(ParseInteger(str, pos));
    case '$':
      return ToVariant(ParseBulkString(str, pos));
    case '*':
      return ToVariant(ParseArray(str, pos));
    default:
      throw std::invalid_argument("Invalid resp type prefix");
  }
}

std::optional<RespValue::RespSimpleString> RespValue::ParseSimpleString(
    const std::string& str, size_t& pos) {
  assert(str[pos] == '+');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    return std::nullopt;  // Missing CLRF
  }
  std::string simple_string = str.substr(pos + 1, end_pos - pos - 1);
  pos = end_pos + 2;  // skip \r\n
  return simple_string;
}

std::optional<RespValue::RespSimpleError> RespValue::ParseSimpleError(
    const std::string& str, size_t& pos) {
  assert(str[pos] == '-');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    return std::nullopt;  // Missing CLRF
  }
  const std::string error_message = str.substr(pos + 1, end_pos - pos - 1);
  pos = end_pos + 2;  // skip \r\n
  return RespSimpleError{.message = error_message};
}

std::optional<RespValue::RespInteger> RespValue::ParseInteger(
    const std::string& str, size_t& pos) {
  assert(str[pos] == ':');
  const size_t end_pos = str.find("\r\n", pos + 1);
  if (end_pos == std::string::npos) {
    return std::nullopt;  // Missing CLRF
  }
  const long long integer_value = ParseDecimal(str, pos + 1, end_pos);
  pos = end_pos + 2;  // skip \r\n
  return integer_value;
}

std::optional<RespValue::RespBulkString> RespValue::ParseBulkString(
    const std::string& str, size_t& pos) {
  assert(str[pos] == '$');
  const size_t end_of_length = str.find("\r\n", pos + 1);
  if (end_of_length == std::string::npos) {
    return std::nullopt;  // Missing CRLF after bulk-string length
  }
  const long long bulk_string_length =
      ParseDecimal(str, pos + 1, end_of_length);
  pos = end_of_length + 2;
  if (bulk_string_length == -1) {
    // Null bulk string: engaged outer optional holding an empty inner one.
    return std::make_optional<RespBulkString>(std::nullopt);
  }
  if (bulk_string_length <= -2) {
    throw std::invalid_argument(
        "Bulk string has a negative length and is not null bulk string");
  }
  // Ensure there's enough data for the bulk string content plus trailing CRLF.
  if (bulk_string_length < 0 ||
      (pos + static_cast<size_t>(bulk_string_length) + 2) > str.size()) {
    return std::nullopt;  // Bulk string payload truncated or missing CRLF
  }
  // Verify terminating CRLF after payload.
  if (str[pos + bulk_string_length] != '\r' ||
      str[pos + bulk_string_length + 1] != '\n') {
    return std::nullopt;  // Bulk string missing terminating CRLF
  }
  std::string bulk_string =
      str.substr(pos, static_cast<size_t>(bulk_string_length));
  pos += static_cast<size_t>(bulk_string_length) + 2;
  return bulk_string;
}

std::optional<RespValue::RespArray> RespValue::ParseArray(
    const std::string& str, size_t& pos) {
  assert(str[pos] == '*');
  const size_t end_of_length = str.find("\r\n", pos + 1);
  if (end_of_length == std::string::npos) {
    return std::nullopt;  // Missing CRLF after array length
  }
  const long long array_length = ParseDecimal(str, pos + 1, end_of_length);
  pos = end_of_length + 2;
  if (array_length < 0) {
    throw std::invalid_argument("Negative array length not allowed");
  }
  std::vector<RespValue> output;
  for (long long i = 0; i < array_length; ++i) {
    if (pos >= str.size()) {
      return std::nullopt;  // Array element missing/truncated
    }
    std::optional<RespValue::RespVariant> array_element =
        ParseVariant(str, pos);
    if (!array_element) {
      return std::nullopt;  // Incomplete array element
    }
    output.push_back(RespValue(*array_element));
  }
  return output;
}

std::string RespValue::Serialize() const {
  StringSink sink;
  SerializeTo(sink);
  return std::move(sink.bytes);
}

void RespValue::SerializeTo(ByteSink& sink) const {
  std::visit(
      [&sink]<typename RespVariant>(const RespVariant& val) {
        using T = std::decay_t<RespVariant>;

        if constexpr (std::is_same_v<T, RespSimpleString>) {
          sink.Append("+");
          sink.Append(val);
          sink.Append("\r\n");
        } else if constexpr (std::is_same_v<T, RespSimpleError>) {
          sink.Append("-");
          sink.Append(val.message);
          sink.Append("\r\n");
        } else if constexpr (std::is_same_v<T, RespInteger>) {
          AppendLine(sink, LinePrefix::kInteger, val);
        } else if constexpr (std::is_same_v<T, RespBulkString>) {
          if (!val.has_value()) {
            sink.Append("$-1\r\n");
            return;
          }
          AppendLine(sink, LinePrefix::kBulkLength,
                     static_cast<long long>(val->length()));
          sink.Append(*val);
          sink.Append("\r\n");
        } else if constexpr (std::is_same_v<T, RespArray>) {
          AppendLine(sink, LinePrefix::kArrayLength,
                     static_cast<long long>(val.size()));
          for (const auto& element : val) {
            element.SerializeTo(sink);
          }
        } else {
          throw std::invalid_argument("Resp Value variant not a valid variant");
        }
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

std::optional<std::pair<RespValue, size_t>> RespValue::FromString(
    const std::string& str) {
  size_t pos = 0;
  std::optional<RespValue::RespVariant> value = ParseVariant(str, pos);
  if (!value) {
    return std::nullopt;  // Incomplete resp value
  }
  return std::make_pair(RespValue(*value), pos);
}

RespValue RespValue::FromVariant(const RespVariant& variant) {
  return RespValue(variant);
}

}  // namespace myredis
