#include "FileLogger.h"

#include <chrono>

namespace {
std::string GetTimeStamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

  const std::tm* tm_ptr = std::localtime(&now_time);
  std::ostringstream oss;
  oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}
}  // namespace

FileLogger::FileLogger(std::ostream& file_stream) : file_stream_(file_stream) {}

void FileLogger::Log(const std::string& input) {
  this->file_stream_ << "LOG [" << GetTimeStamp() << "]: " << input
                     << std::endl;
}

void FileLogger::Debug(const std::string& input) {
  this->file_stream_ << "DEBUG [" << GetTimeStamp() << "]: " << input
                     << std::endl;
}
void FileLogger::Error(const std::string& input) {
  this->file_stream_ << "ERROR [" << GetTimeStamp() << "]: " << input
                     << std::endl;
}