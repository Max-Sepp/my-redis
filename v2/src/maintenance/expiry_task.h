#ifndef MYREDIS_MAINTENANCE_EXPIRY_TASK_H_
#define MYREDIS_MAINTENANCE_EXPIRY_TASK_H_

#include <cstddef>
#include <functional>
#include <memory>

#include "maintenance/background_task.h"
#include "store/store.h"

namespace myredis {

// Active expiry as a BackgroundTask. Each slice runs the store's
// ActiveExpireCycle, sampling another round while the slice has time and the
// last round suggests a backlog (more than 1/kBacklogRatio of it had
// expired). Reports pending work while that backlog remains.
class ExpiryTask final : public BackgroundTask {
 public:
  explicit ExpiryTask(const std::unique_ptr<Store>& store) : store_(store) {}

  bool RunSlice(const std::function<bool()>& has_time) override {
    const Store::ExpireCycleResult result =
        store_->ActiveExpireCycle([&](const Store::ExpireRound& round) {
          return HasBacklog(round) && has_time();
        });
    return !result.index_empty && HasBacklog(result.last_round);
  }

 private:
  static constexpr std::size_t kBacklogRatio = 4;

  static bool HasBacklog(const Store::ExpireRound& round) {
    return round.removed * kBacklogRatio > round.sampled;
  }

  const std::unique_ptr<Store>& store_;
};

}  // namespace myredis

#endif  // MYREDIS_MAINTENANCE_EXPIRY_TASK_H_
