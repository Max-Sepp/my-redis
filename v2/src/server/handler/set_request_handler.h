#ifndef MYREDIS_SERVER_HANDLER_SET_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_SET_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"
#include "store/store.h"

namespace myredis {

// SET <key> <value>: stores value under key and replies +OK.
class SetRequestHandler {
 public:
  static constexpr std::string_view kName = "SET";

  explicit SetRequestHandler(const std::unique_ptr<Store>& store)
      : store_(store) {}

  [[nodiscard]] static bool IsHandler(const RespValue& request) {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == kName && command->args.size() == 2 &&
           KeyArg(*command, 0) != nullptr;
  }

  void Handle(const RespValue& request, RespWriter& out) {
    const std::optional<Command> command = ParseCommand(request);
    if (!command || command->args.size() != 2) {
      out.Error("ERR syntax error");
      return;
    }
    const std::string* key = KeyArg(*command, 0);
    if (key == nullptr) {
      out.Error("ERR syntax error");
      return;
    }
    store_->Set(*key, command->args[1]);
    out.SimpleString("OK");
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

static_assert(RequestHandler<SetRequestHandler>);

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_SET_REQUEST_HANDLER_H_
