#ifndef MYREDIS_SERVER_HANDLER_DEL_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_DEL_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"
#include "store/store.h"

namespace myredis {

// DEL <key>: removes key from the store and replies :1.
class DelRequestHandler final : public Handler {
 public:
  explicit DelRequestHandler(const std::unique_ptr<Store>& store)
      : store_(store) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "DEL" && command->args.size() == 1 &&
           KeyArg(*command, 0) != nullptr;
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    const std::string* key = command ? KeyArg(*command, 0) : nullptr;
    if (key == nullptr) return Error("ERR syntax error");
    store_->Del(*key);
    return Integer(1);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_DEL_REQUEST_HANDLER_H_
