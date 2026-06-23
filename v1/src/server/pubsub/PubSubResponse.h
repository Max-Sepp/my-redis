#ifndef MY_REDIS_PUBSUBRESPONSE_H
#define MY_REDIS_PUBSUBRESPONSE_H

#include <string>

#include "respvalue/RespValue.h"

RespValue PubSubResponse(const std::string& action, const std::string& channel,
                         const RespValue& contents);

#endif  // MY_REDIS_PUBSUBRESPONSE_H
