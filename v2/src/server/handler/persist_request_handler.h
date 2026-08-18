#ifndef MYREDIS_SERVER_HANDLER_PERSIST_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_PERSIST_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"
#include "store/store.h"

namespace myredis {

// PERSIST <key>: removes key's TTL and replies :1, or :0 if the key doesn't
// exist or has no TTL.
class PersistRequestHandler final : public Handler {
 public:
  explicit PersistRequestHandler(const std::unique_ptr<Store>& store)
      : store_(store) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "PERSIST" &&
           command->args.size() == 1 && command->args[0].has_value() &&
           !command->args[0]->empty();
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    const bool persisted = store_->Persist(*command->args[0]);
    return Integer(persisted ? 1 : 0);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_PERSIST_REQUEST_HANDLER_H_
