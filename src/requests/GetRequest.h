#ifndef MY_REDIS_GETREQUEST_H
#define MY_REDIS_GETREQUEST_H

#include <string>

#include "respvalue/RespValue.h"

class GetRequest {
 public:
  explicit GetRequest(const RespValue& resp_value);
  [[nodiscard]] const std::string& getKey() const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  std::string key_;
};

#endif  // MY_REDIS_GETREQUEST_H
