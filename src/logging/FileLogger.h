#ifndef MY_REDIS_CONSOLELOGGER_H
#define MY_REDIS_CONSOLELOGGER_H

#include <fstream>
#include <string>

#include "Logger.h"

class FileLogger final : public Logger {
 public:
  explicit FileLogger(std::ostream& file_stream);

  void Log(const std::string&) override;

  void Debug(const std::string&) override;

  void Error(const std::string&) override;

 private:
  std::ostream& file_stream_;
};

#endif  // MY_REDIS_CONSOLELOGGER_H
