#include "GetRequest.h"

#include <stdexcept>

GetRequest::GetRequest(const RespValue& resp_value)
    : one_arg_request_(resp_value, "GET") {}

const std::string& GetRequest::getKey() const {
  return this->one_arg_request_.getFirstArg();
}

bool GetRequest::IsRequest(const RespValue& resp_value) {
  return OneArgRequest("GET").IsRequest(resp_value);
}
