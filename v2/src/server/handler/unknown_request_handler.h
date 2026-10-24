#ifndef MYREDIS_SERVER_HANDLER_UNKNOWN_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_UNKNOWN_REQUEST_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
#include "server/handler/handler.h"

namespace myredis {

// Fallback handler: the dispatcher's reply for any request with an
// unrecognised command keyword, or one its handler rejected.
class UnknownRequestHandler {
 public:
  static void Handle(const RespValue& /*request*/, RespWriter& out) {
    out.Error("Unknown subcommand or command");
  }
};

static_assert(FallbackHandler<UnknownRequestHandler>);

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_UNKNOWN_REQUEST_HANDLER_H_
