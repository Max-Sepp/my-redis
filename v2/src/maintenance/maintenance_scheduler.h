#ifndef MYREDIS_MAINTENANCE_MAINTENANCE_SCHEDULER_H_
#define MYREDIS_MAINTENANCE_MAINTENANCE_SCHEDULER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "maintenance/background_task.h"
#include "time/time.h"

namespace myredis {

// Runs BackgroundTasks in time-bounded slices. The server calls RunSlice from
// two places: a periodic timer, so tasks always get a little time even under
// sustained load, and whenever its event loop is idle while HasPendingWork(),
// so a backlog is worked off during gaps in traffic.
class MaintenanceScheduler {
 public:
  explicit MaintenanceScheduler(std::unique_ptr<Time> time);

  void Add(std::unique_ptr<BackgroundTask> task);

  // Gives every task a turn, sharing budget_ms between them. The task that
  // goes first rotates each slice so one busy task cannot always take the
  // whole budget. Returns HasPendingWork().
  bool RunSlice(std::int64_t budget_ms);

  // True if any task reported pending work on its last turn.
  [[nodiscard]] bool HasPendingWork() const;

 private:
  std::unique_ptr<Time> time_;
  std::vector<std::unique_ptr<BackgroundTask>> tasks_;
  // pending_[i] is what tasks_[i] returned on its last turn.
  std::vector<bool> pending_;
  std::size_t next_first_task_ = 0;
};

}  // namespace myredis

#endif  // MYREDIS_MAINTENANCE_MAINTENANCE_SCHEDULER_H_
