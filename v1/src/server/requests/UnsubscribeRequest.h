#ifndef MY_REDIS_UNSUBSCRIBEREQUEST_H
#define MY_REDIS_UNSUBSCRIBEREQUEST_H

#include "VarArgRequest.h"
#include "respvalue/RespValue.h"

class UnsubscribeRequest {
 public:
  explicit UnsubscribeRequest(const RespValue& resp_value);
  [[nodiscard]] int numChannels() const;
  [[nodiscard]] const std::string& getChannel(int index) const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  VarArgRequest var_arg_request_;
};

#endif  // MY_REDIS_UNSUBSCRIBEREQUEST_H
