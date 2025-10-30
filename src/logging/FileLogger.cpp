#include "FileLogger.h"

#include <chrono>

namespace {
std::string GetTimeStamp() {
  const auto now = std::chrono::system_clock::now();

  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

  const std::tm* tm_ptr = std::localtime(&now_time);

  const auto duration = now.time_since_epoch();

  const auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(duration);
  const auto microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(duration) -
      std::chrono::duration_cast<std::chrono::microseconds>(seconds);
  const auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration) -
      std::chrono::duration_cast<std::chrono::nanoseconds>(seconds);

  // Format the timestamp
  std::ostringstream oss;
  oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(6)
      << std::setfill('0') << microseconds.count() << '.' << std::setw(9)
      << std::setfill('0') << nanoseconds.count();

  return oss.str();
}

std::string EscapeDelimitedChars(const std::string& input) {
  std::string output;
  for (char c : input) {
    switch (c) {
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      case '\v':
        output += "\\v";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\a':
        output += "\\a";
        break;
      case '\b':
        output += "\\b";
        break;
      case '\\':
        output += "\\\\";
        break;
      case '\"':
        output += "\\\"";
        break;
      case '\'':
        output += "\\\'";
        break;
      default:
        output += c;
    }
  }
  return output;
}
}  // namespace

FileLogger::FileLogger(std::ostream& file_stream) : file_stream_(file_stream) {}

void FileLogger::Log(const std::string& input) {
  this->file_stream_ << "LOG [" << GetTimeStamp()
                     << "]: " << EscapeDelimitedChars(input) << std::endl;
}

void FileLogger::Debug(const std::string& input) {
  this->file_stream_ << "DEBUG [" << GetTimeStamp()
                     << "]: " << EscapeDelimitedChars(input) << std::endl;
}
void FileLogger::Error(const std::string& input) {
  this->file_stream_ << "ERROR [" << GetTimeStamp()
                     << "]: " << EscapeDelimitedChars(input) << std::endl;
}