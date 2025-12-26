#ifndef MY_REDIS_SETREQUEST_H
#define MY_REDIS_SETREQUEST_H

#include "respvalue/RespValue.h"

class SetRequest {
 public:
  explicit SetRequest(const RespValue& resp_value);
  [[nodiscard]] const std::string& getKey() const;
  [[nodiscard]] const std::optional<std::string>& getValue() const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  std::string key_;
  std::optional<std::string> value_;
};

#endif  // MY_REDIS_SETREQUEST_H
