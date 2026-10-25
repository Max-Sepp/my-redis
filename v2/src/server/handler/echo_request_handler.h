#ifndef MYREDIS_SERVER_HANDLER_ECHO_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_ECHO_REQUEST_HANDLER_H_

#include <optional>
#include <string>
#include <string_view>

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"

namespace myredis {

class EchoRequestHandler {
 public:
  static constexpr std::string_view kName = "ECHO";

  [[nodiscard]] static bool IsHandler(const RespValue& request) {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == kName && command->args.size() == 1;
  }

  static void Handle(const RespValue& request, RespWriter& out) {
    const std::optional<Command> command = ParseCommand(request);
    if (!command || command->args.size() != 1) {
      out.Error("ERR syntax error");
      return;
    }
    const std::optional<std::string>& arg = command->args[0];
    if (!arg.has_value()) {
      out.NullBulkString();
      return;
    }
    out.BulkString(*arg);
  }
};

static_assert(RequestHandler<EchoRequestHandler>);

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_ECHO_REQUEST_HANDLER_H_
