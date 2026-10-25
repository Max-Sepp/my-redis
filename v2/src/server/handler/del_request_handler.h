#ifndef MYREDIS_SERVER_HANDLER_DEL_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_DEL_REQUEST_HANDLER_H_

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

// DEL <key>: removes key from the store and replies :1.
class DelRequestHandler {
 public:
  static constexpr std::string_view kName = "DEL";

  explicit DelRequestHandler(const std::unique_ptr<Store>& store)
      : store_(store) {}

  [[nodiscard]] static bool IsHandler(const RespValue& request) {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == kName && command->args.size() == 1 &&
           KeyArg(*command, 0) != nullptr;
  }

  void Handle(const RespValue& request, RespWriter& out) {
    const std::optional<Command> command = ParseCommand(request);
    const std::string* key = command ? KeyArg(*command, 0) : nullptr;
    if (key == nullptr) {
      out.Error("ERR syntax error");
      return;
    }
    store_->Del(*key);
    out.Integer(1);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

static_assert(RequestHandler<DelRequestHandler>);

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_DEL_REQUEST_HANDLER_H_
