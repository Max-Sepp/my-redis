#ifndef MYREDIS_RESP_VALUE_RESP_VALUES_H_
#define MYREDIS_RESP_VALUE_RESP_VALUES_H_

#include "resp_value.h"

namespace myredis {

RespValue NullBulkString();
RespValue BulkString(const std::optional<std::string>& string);
RespValue Integer(long long num);
RespValue Error(const std::string& message);
RespValue Array(const std::vector<RespValue>& resp_values);

}  // namespace myredis

#endif  // MYREDIS_RESP_VALUE_RESP_VALUES_H_
