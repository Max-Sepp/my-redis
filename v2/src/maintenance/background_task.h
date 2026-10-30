#ifndef MYREDIS_MAINTENANCE_BACKGROUND_TASK_H_
#define MYREDIS_MAINTENANCE_BACKGROUND_TASK_H_

#include <functional>

namespace myredis {

// Housekeeping that runs on the main thread in short, time-bounded slices
// between commands (e.g. active expiry; later, incremental rehashing). A task
// must be safe to interrupt after any slice and resume on the next one.
class BackgroundTask {
 public:
  virtual ~BackgroundTask() = default;

  // Does some work, checking has_time() between bounded steps and stopping
  // once it returns false. A task should still make some progress even if
  // has_time() is false from the start, so it cannot starve. Returns true if
  // it has work pending and wants another slice soon; false if it is caught up
  // (it will still be run on the scheduler's periodic tick to notice new work).
  virtual bool RunSlice(const std::function<bool()>& has_time) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_MAINTENANCE_BACKGROUND_TASK_H_
