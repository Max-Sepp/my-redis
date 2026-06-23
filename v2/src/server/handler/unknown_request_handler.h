#ifndef MYREDIS_SERVER_HANDLER_UNKNOWN_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_UNKNOWN_REQUEST_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/handler.h"

namespace myredis {

// Fallback handler: matches any request the earlier handlers rejected and
// replies with an error. Must be last in the dispatch chain.
class UnknownRequestHandler final : public Handler {
 public:
  [[nodiscard]] bool IsHandler(const RespValue& /*request*/) const override {
    return true;
  }

  [[nodiscard]] RespValue Handle(const RespValue& /*request*/) override {
    return Error("Unknown subcommand or command");
  }
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_UNKNOWN_REQUEST_HANDLER_H_
