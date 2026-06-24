#ifndef MYREDIS_SERVER_HANDLER_SET_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_SET_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"
#include "store/map.h"

namespace myredis {

// SET <key> <value>: stores value under key and replies +OK.
class SetRequestHandler final : public Handler {
 public:
  explicit SetRequestHandler(
      const std::unique_ptr<Map<std::string, std::optional<std::string>>>&
          store)
      : store_(store) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == "SET" && command->args.size() == 2 &&
           command->args[0].has_value() && !command->args[0]->empty();
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    store_->Insert(*command->args[0], command->args[1]);
    return SimpleString("OK");
  }

 private:
  // Bound to the server's store handle, not the map itself, so the reference
  // stays valid even if the underlying map is replaced (e.g. snapshot restore).
  const std::unique_ptr<Map<std::string, std::optional<std::string>>>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_SET_REQUEST_HANDLER_H_
