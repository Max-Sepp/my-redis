#ifndef MYREDIS_SERVER_HANDLER_TTL_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_TTL_REQUEST_HANDLER_H_

#include <cstdint>
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

// TTL/PTTL <key>: replies with key's remaining TTL in seconds/milliseconds
// respectively, -1 if it has none, or -2 if the key doesn't exist.
//
// `kTimeConversionFactor` converts milliseconds to the reply's unit.
template <FixedString kCommandName, int kTimeConversionFactor>
class TtlRequestHandler {
 public:
  static constexpr std::string_view kName = kCommandName.View();

  explicit TtlRequestHandler(const std::unique_ptr<Store>& store)
      : store_(store) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const {
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
    const std::int64_t result = store_->Ttl(*key);
    // -1 (no TTL) and -2 (no key) are sentinels, not durations — don't
    // convert them.
    out.Integer(result < 0 ? result : result / kTimeConversionFactor);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_TTL_REQUEST_HANDLER_H_
