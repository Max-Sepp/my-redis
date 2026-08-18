#ifndef MYREDIS_SERVER_HANDLER_TTL_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_TTL_REQUEST_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "resp_value/resp_value.h"
#include "resp_value/resp_values.h"
#include "server/handler/command.h"
#include "server/handler/handler.h"
#include "store/store.h"

namespace myredis {

class TtlRequestHandler final : public Handler {
 public:
  explicit TtlRequestHandler(std::string handler_name,
                             const int time_conversion_factor,
                             const std::unique_ptr<Store>& store)
      : store_(store),
        handler_name_(std::move(handler_name)),
        time_conversion_factor_(time_conversion_factor) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == handler_name_ &&
           command->args.size() == 1 && command->args[0].has_value() &&
           !command->args[0]->empty();
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    const std::int64_t result = store_->Ttl(*command->args[0]);
    // -1 (no TTL) and -2 (no key) are sentinels, not durations — don't
    // convert them.
    return Integer(result < 0 ? result : result / time_conversion_factor_);
  }

 private:
  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;

  const std::string handler_name_;
  const int time_conversion_factor_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_TTL_REQUEST_HANDLER_H_
