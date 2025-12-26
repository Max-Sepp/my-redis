#include "DelRequest.h"

DelRequest::DelRequest(const RespValue& resp_value)
    : one_arg_request_(resp_value, "DEL") {}

const std::string& DelRequest::getKey() const {
  return this->one_arg_request_.getFirstArg();
}

bool DelRequest::IsRequest(const RespValue& resp_value) {
  return OneArgRequest("DEL").IsRequest(resp_value);
}
