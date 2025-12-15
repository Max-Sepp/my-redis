#ifndef MY_REDIS_LOGGER_H
#define MY_REDIS_LOGGER_H

#include <string>

class Logger {
 public:
  virtual ~Logger() = default;

  virtual void Log(const std::string&) = 0;

  virtual void Debug(const std::string&) = 0;

  virtual void Error(const std::string&) = 0;
};

#endif  // MY_REDIS_LOGGER_H
