#ifndef MY_REDIS_GETREQUEST_H
#define MY_REDIS_GETREQUEST_H

#include "OneArgRequest.h"
#include "respvalue/RespValue.h"

class GetRequest {
 public:
  explicit GetRequest(const RespValue& resp_value);
  [[nodiscard]] const std::string& getKey() const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  OneArgRequest one_arg_request_;
};

#endif  // MY_REDIS_GETREQUEST_H
