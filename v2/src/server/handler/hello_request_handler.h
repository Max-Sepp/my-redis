#ifndef MYREDIS_SERVER_HANDLER_HELLO_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_HELLO_REQUEST_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"

namespace myredis {

// HELLO [protover]: negotiates the protocol version and replies with server
// info. Only RESP2 (`resp_value/`) is implemented, so the only accepted
// protover is "2"; anything else (notably "3") is rejected the way real
// Redis rejects protocol versions the server doesn't speak.
class HelloRequestHandler final : public Handler {
  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "HELLO" && command->args.size() <= 1;
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    if (command->args.size() == 1 && command->args[0] != "2") {
      return Error(
          "NOPROTO unsupported protocol version");
    }
    return Array({
        BulkString("server"),
        BulkString("myredis"),
        BulkString("version"),
        BulkString("0.1.0"),
        BulkString("proto"),
        Integer(2),
        BulkString("mode"),
        BulkString("standalone"),
        BulkString("role"),
        BulkString("master"),
        BulkString("modules"),
        Array({}),
    });
  }
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_HELLO_REQUEST_HANDLER_H_
