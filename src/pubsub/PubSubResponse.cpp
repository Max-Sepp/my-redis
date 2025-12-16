#include "PubSubResponse.h"

#include "respvalue/RespValues.h"

RespValue PubSubResponse(const std::string& action, const std::string& channel,
                         const int num_open_channels) {
  return Array(
      {BulkString(action), BulkString(channel), Integer(num_open_channels)});
}