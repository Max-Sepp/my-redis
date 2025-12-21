#include "UnsubscribeRequest.h"

UnsubscribeRequest::UnsubscribeRequest(const RespValue& resp_value)
    : var_arg_request_(resp_value, "UNSUBSCRIBE") {}

int UnsubscribeRequest::numChannels() const {
  return var_arg_request_.numArgs();
}

const std::string& UnsubscribeRequest::getChannel(const int index) const {
  return var_arg_request_.getArg(index);
}

bool UnsubscribeRequest::IsRequest(const RespValue& resp_value) {
  return VarArgRequest("UNSUBSCRIBE").IsRequest(resp_value);
}
