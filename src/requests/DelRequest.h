#ifndef MY_REDIS_DELREQUEST_H
#define MY_REDIS_DELREQUEST_H

#include "OneArgRequest.h"
#include "respvalue/RespValue.h"

class DelRequest {
 public:
  explicit DelRequest(const RespValue& resp_value);
  [[nodiscard]] const std::string& getKey() const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  OneArgRequest one_arg_request_;
};

#endif  // MY_REDIS_DELREQUEST_H
