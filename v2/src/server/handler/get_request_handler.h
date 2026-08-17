#ifndef MYREDIS_SERVER_HANDLER_GET_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_GET_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"
#include "store/store.h"

namespace myredis {

// GET <key>: returns the stored bulk string, or a null bulk string if the key
// is absent (or was stored with a null value).
class GetRequestHandler final : public Handler {
 public:
  explicit GetRequestHandler(const std::unique_ptr<Store>& store)
      : store_(store) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "GET" && command->args.size() == 1 &&
           command->args[0].has_value() && !command->args[0]->empty();
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    const std::optional<std::string> found = store_->Get(*command->args[0]);
    if (!found.has_value()) return NullBulkString();
    return BulkString(*found);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_GET_REQUEST_HANDLER_H_
