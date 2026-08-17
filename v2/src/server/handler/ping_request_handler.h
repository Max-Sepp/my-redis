#ifndef MYREDIS_SERVER_HANDLER_PING_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_PING_REQUEST_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"

namespace myredis {

class PingRequestHandler final : public Handler {
  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "PING" &&
           (command->args.size() == 0 || command->args.size() == 1);
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    if (command->args.size() == 0) {
      return SimpleString("PONG");
    }
    return BulkString(command->args[0]);
  }
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_PING_REQUEST_HANDLER_H_
