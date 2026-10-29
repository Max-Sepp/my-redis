#include "maintenance/maintenance_scheduler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace myredis {

MaintenanceScheduler::MaintenanceScheduler(std::unique_ptr<Time> time)
    : time_(std::move(time)) {}

void MaintenanceScheduler::Add(std::unique_ptr<BackgroundTask> task) {
  tasks_.push_back(std::move(task));
  pending_.push_back(false);
}

bool MaintenanceScheduler::RunSlice(const std::int64_t budget_ms) {
  const std::int64_t start_ms = time_->NowMs();
  const auto has_time = [&] { return time_->NowMs() - start_ms < budget_ms; };

  for (std::size_t offset = 0; offset < tasks_.size(); ++offset) {
    const std::size_t index = (next_first_task_ + offset) % tasks_.size();
    pending_[index] = tasks_[index]->RunSlice(has_time);
  }
  if (!tasks_.empty()) {
    next_first_task_ = (next_first_task_ + 1) % tasks_.size();
  }
  return HasPendingWork();
}

bool MaintenanceScheduler::HasPendingWork() const {
  return std::ranges::any_of(pending_,
                             [](const bool pending) { return pending; });
}

}  // namespace myredis
