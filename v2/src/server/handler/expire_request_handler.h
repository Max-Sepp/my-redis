#ifndef MYREDIS_SERVER_HANDLER_EXPIRE_REQUEST_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_EXPIRE_REQUEST_HANDLER_H_

#include <charconv>
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

// EXPIRE/PEXPIRE <key> <ttl> [NX|XX|GT|LT]: sets key's TTL relative to now
// (ttl in seconds/milliseconds respectively).
// EXPIREAT/PEXPIREAT <key> <timestamp> [NX|XX|GT|LT]: sets key's TTL to an
// absolute unix timestamp (in seconds/milliseconds respectively).
// All four reply :1, or :0 if the key doesn't exist or the option's
// condition isn't met.
class ExpireRequestHandler final : public Handler {
 public:
  explicit ExpireRequestHandler(std::string handler_name,
                                const int time_conversion_factor,
                                const bool relative_to_now,
                                const std::unique_ptr<Store>& store)
      : store_(store),
        handler_name_(std::move(handler_name)),
        time_conversion_factor_(time_conversion_factor),
        relative_to_now_(relative_to_now) {}

  [[nodiscard]] bool IsHandler(const RespValue& request) const override {
    const std::optional<Command> command = ParseCommand(request);
    return command && command->name == handler_name_ &&
           (command->args.size() == 2 || command->args.size() == 3) &&
           command->args[0].has_value() && !command->args[0]->empty() &&
           ParseInteger(command->args[1]).has_value() &&
           (command->args.size() == 2 ||
            Store::ToExpireOption(*command->args[2]).has_value());
  }

  [[nodiscard]] RespValue Handle(const RespValue& request) override {
    const std::optional<Command> command = ParseCommand(request);
    const int64_t value = *ParseInteger(command->args[1]);
    const Store::ExpireOption option =
        command->args.size() == 3 ? *Store::ToExpireOption(*command->args[2])
                                  : Store::ExpireOption::NA;
    const int64_t base = relative_to_now_ ? store_->NowMs() : 0;
    const int64_t proposed_expiry = base + (value * time_conversion_factor_);
    const bool set =
        store_->ExpireAt(*command->args[0], proposed_expiry, option);
    return Integer(set ? 1 : 0);
  }

 private:
  static std::optional<int64_t> ParseInteger(
      const std::optional<std::string>& arg) {
    if (!arg.has_value() || arg->empty()) return std::nullopt;
    int64_t value = 0;
    const auto [ptr, errc] =
        std::from_chars(arg->data(), arg->data() + arg->size(), value);
    if (errc != std::errc() || ptr != arg->data() + arg->size()) {
      return std::nullopt;
    }
    return value;
  }

  // Bound to the server's store handle, not the store itself, so the
  // reference stays valid even if the store's contents are replaced (e.g.
  // snapshot restore).
  const std::unique_ptr<Store>& store_;

  const std::string handler_name_;
  const int time_conversion_factor_;
  const bool relative_to_now_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_EXPIRE_REQUEST_HANDLER_H_
