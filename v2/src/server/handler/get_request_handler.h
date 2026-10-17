#ifndef MYREDIS_SERVER_HANDLER_GET_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_GET_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
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
           KeyArg(*command, 0) != nullptr;
  }

  void Handle(const RespValue& request, RespWriter& out) override {
    const std::optional<Command> command = ParseCommand(request);
    const std::string* key = command ? KeyArg(*command, 0) : nullptr;
    if (key == nullptr) {
      out.Error("ERR syntax error");
      return;
    }
    const std::optional<std::string> found = store_->Get(*key);
    if (!found.has_value()) {
      out.NullBulkString();
      return;
    }
    out.BulkString(*found);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_GET_REQUEST_HANDLER_H_
