#ifndef MY_REDIS_ONEARGREQUEST_H
#define MY_REDIS_ONEARGREQUEST_H
#include <string>

#include "respvalue/RespValue.h"

class OneArgRequest {
 public:
  OneArgRequest(const RespValue& resp_value, const std::string& request_type);
  explicit OneArgRequest(const std::string& request_type);

  [[nodiscard]] const std::string& getFirstArg() const;
  [[nodiscard]] bool IsRequest(const RespValue& resp_value) const;

 private:
  std::string first_arg_;
  std::string request_type_;
};

#endif  // MY_REDIS_ONEARGREQUEST_H
