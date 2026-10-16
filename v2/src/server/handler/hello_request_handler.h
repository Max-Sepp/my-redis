#ifndef MYREDIS_SERVER_HANDLER_HELLO_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_HELLO_REQUEST_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
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

  void Handle(const RespValue& request, RespWriter& out) override {
    const std::optional<Command> command = ParseCommand(request);
    if (command->args.size() == 1 && command->args[0] != "2") {
      out.Error("NOPROTO unsupported protocol version");
      return;
    }
    // Six key/value pairs.
    out.ArrayHeader(12);
    out.BulkString("server");
    out.BulkString("myredis");
    out.BulkString("version");
    out.BulkString("0.1.0");
    out.BulkString("proto");
    out.Integer(2);
    out.BulkString("mode");
    out.BulkString("standalone");
    out.BulkString("role");
    out.BulkString("master");
    out.BulkString("modules");
    out.ArrayHeader(0);
  }
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_HELLO_REQUEST_HANDLER_H_
