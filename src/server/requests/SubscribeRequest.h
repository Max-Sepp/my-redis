#ifndef MY_REDIS_SUBSCRIBEREQUEST_H
#define MY_REDIS_SUBSCRIBEREQUEST_H
#include "VarArgRequest.h"
#include "respvalue/RespValue.h"

class SubscribeRequest {
 public:
  explicit SubscribeRequest(const RespValue& resp_value);
  [[nodiscard]] int numChannels() const;
  [[nodiscard]] const std::string& getChannel(int index) const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  VarArgRequest var_arg_request_;
};

#endif  // MY_REDIS_SUBSCRIBEREQUEST_H
