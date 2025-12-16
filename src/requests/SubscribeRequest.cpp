#include "SubscribeRequest.h"

SubscribeRequest::SubscribeRequest(const RespValue& resp_value)
    : var_arg_request_(resp_value, "SUBSCRIBE") {}

int SubscribeRequest::numChannels() const { return var_arg_request_.numArgs(); }

const std::string& SubscribeRequest::getChannel(const int index) const {
  return var_arg_request_.getArg(index);
}

bool SubscribeRequest::IsRequest(const RespValue& resp_value) {
  return VarArgRequest("SUBSCRIBE").IsRequest(resp_value);
}
