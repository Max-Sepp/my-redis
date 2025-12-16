#ifndef MY_REDIS_VARARGREQUEST_H
#define MY_REDIS_VARARGREQUEST_H

#include <string>

#include "respvalue/RespValue.h"

class VarArgRequest {
 public:
  VarArgRequest(const RespValue& resp_value, std::string request_type);
  explicit VarArgRequest(std::string request_type);

  [[nodiscard]] int numArgs() const;
  [[nodiscard]] const std::string& getArg(int index) const;
  [[nodiscard]] bool IsRequest(const RespValue& resp_value) const;

 private:
  std::vector<std::string> args_;
  std::string request_type_;
};

#endif  // MY_REDIS_VARARGREQUEST_H
