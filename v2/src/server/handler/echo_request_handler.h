#ifndef MYREDIS_SERVER_HANDLER_ECHO_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_ECHO_REQUEST_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"

namespace myredis {

class EchoRequestHandler final : public Handler {
  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "ECHO" && command->args.size() == 1;
  }

  void Handle(const RespValue& request, RespWriter& out) override {
    const std::optional<Command> command = ParseCommand(request);
    const std::optional<std::string>& arg = command->args[0];
    if (!arg.has_value()) {
      out.NullBulkString();
      return;
    }
    out.BulkString(*arg);
  }
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_ECHO_REQUEST_HANDLER_H_
