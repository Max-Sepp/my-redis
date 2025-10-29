#ifndef MY_REDIS_NULLBULKSTRING_H
#define MY_REDIS_NULLBULKSTRING_H
#include "RespValue.h"

RespValue NullBulkString();
RespValue BulkString(const std::optional<std::string>& string);

#endif  // MY_REDIS_NULLBULKSTRING_H
