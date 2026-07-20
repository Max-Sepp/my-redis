#ifndef MY_REDIS_NULLLOGGER_H
#define MY_REDIS_NULLLOGGER_H

#include <string>

#include "Logger.h"

// A Logger that discards everything. Used to disable logging entirely (the
// per-request FileLogger writes dominate the server's latency), while keeping
// the Logger dependency wired through the handlers unchanged.
class NullLogger final : public Logger {
 public:
  void Log(const std::string& /*message*/) override {}

  void Debug(const std::string& /*message*/) override {}

  void Error(const std::string& /*message*/) override {}
};

#endif  // MY_REDIS_NULLLOGGER_H
