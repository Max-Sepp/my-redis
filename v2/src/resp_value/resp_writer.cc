#include "resp_value/resp_writer.h"

#include <array>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <limits>
#include <system_error>

namespace myredis {

namespace {

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

}  // namespace

void RespWriter::SimpleString(const std::string_view string) {
  sink_.Append("+");
  sink_.Append(string);
  sink_.Append("\r\n");
}

void RespWriter::Error(const std::string_view message) {
  sink_.Append("-");
  sink_.Append(message);
  sink_.Append("\r\n");
}

void RespWriter::Integer(const long long num) {
  AppendLine(sink_, LinePrefix::kInteger, num);
}

void RespWriter::BulkString(const std::string_view string) {
  AppendLine(sink_, LinePrefix::kBulkLength,
             static_cast<long long>(string.size()));
  sink_.Append(string);
  sink_.Append("\r\n");
}

void RespWriter::NullBulkString() { sink_.Append("$-1\r\n"); }

void RespWriter::ArrayHeader(const std::size_t size) {
  AppendLine(sink_, LinePrefix::kArrayLength, static_cast<long long>(size));
}

}  // namespace myredis
