#include "PubSubResponse.h"

#include "respvalue/RespValues.h"

RespValue PubSubResponse(const std::string& action, const std::string& channel,
                         const RespValue& contents) {
  return Array({BulkString(action), BulkString(channel), contents});
}
