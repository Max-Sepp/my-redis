#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "maintenance/background_task.h"
#include "maintenance/expiry_task.h"
#include "maintenance/maintenance_scheduler.h"
#include "store/store.h"
#include "time/time.h"

using myredis::BackgroundTask;
using myredis::ExpiryTask;
using myredis::MaintenanceScheduler;
using myredis::Store;
using myredis::Time;

namespace {

constexpr std::uint64_t kSeed = 42;
constexpr std::int64_t kStartMs = 1000;
constexpr std::int64_t kTtlMs = 500;
constexpr int kNumKeys = 1000;

// A clock the test advances by hand, or by step_ms on every read.
class FakeTime final : public Time {
 public:
  std::int64_t NowMs() override {
    const std::int64_t now = now_ms;
    now_ms += step_ms;
    return now;
  }
  std::int64_t now_ms = kStartMs;
  std::int64_t step_ms = 0;
};

const std::function<bool()> kAlwaysHasTime = [] { return true; };
const std::function<bool()> kNeverHasTime = [] { return false; };

class ExpiryTaskTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto time = std::make_unique<FakeTime>();
    time_ = time.get();
    store_ = std::make_unique<Store>(std::move(time), kSeed);
  }

  void SetKeys(const int count, const bool with_ttl) {
    for (int i = 0; i < count; ++i) {
      const std::string key =
          (with_ttl ? "volatile-" : "live-") + std::to_string(i);
      store_->Set(key, "value");
      if (with_ttl) store_->ExpireAt(key, kStartMs + kTtlMs);
    }
  }

  void AdvancePastTtl() { time_->now_ms = kStartMs + kTtlMs + 1; }

  FakeTime* time_ = nullptr;
  std::unique_ptr<Store> store_;
};

}  // namespace

TEST_F(ExpiryTaskTest, PendingWhileBacklogRemains) {
  SetKeys(kNumKeys, true);
  AdvancePastTtl();
  ExpiryTask task(store_);

  // Out of time after the first round, which was entirely expired.
  EXPECT_TRUE(task.RunSlice(kNeverHasTime));
}

TEST_F(ExpiryTaskTest, CaughtUpOnceIndexEmpties) {
  SetKeys(kNumKeys, true);
  AdvancePastTtl();
  ExpiryTask task(store_);

  EXPECT_FALSE(task.RunSlice(kAlwaysHasTime));
  EXPECT_EQ(store_->SerialiseToJson(), "{}");
}

TEST_F(ExpiryTaskTest, NotPendingWhenKeysAreLive) {
  SetKeys(kNumKeys, true);
  ExpiryTask task(store_);

  EXPECT_FALSE(task.RunSlice(kAlwaysHasTime));
}

TEST_F(ExpiryTaskTest, BacklogDrainsOverSlices) {
  SetKeys(kNumKeys, true);
  AdvancePastTtl();
  ExpiryTask task(store_);

  // Each slice runs exactly one round, removing 20 keys, as an idle loop
  // would. The last full round still looks like a backlog, so one more slice
  // finds the index empty.
  int slices = 1;
  while (task.RunSlice(kNeverHasTime)) slices++;

  EXPECT_EQ(slices, (kNumKeys / 20) + 1);
  EXPECT_EQ(store_->SerialiseToJson(), "{}");
}

namespace {

// Records each turn it gets and reports a scripted pending value.
class FakeTask final : public BackgroundTask {
 public:
  FakeTask(std::string name, std::vector<std::string>& turns)
      : name_(std::move(name)), turns_(turns) {}

  bool RunSlice(const std::function<bool()>& has_time) override {
    turns_.push_back(name_);
    while (has_time()) time_checks++;
    return pending;
  }

  bool pending = false;
  int time_checks = 0;

 private:
  std::string name_;
  std::vector<std::string>& turns_;
};

class MaintenanceSchedulerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto time = std::make_unique<FakeTime>();
    time_ = time.get();
    // FakeTask spins on has_time(), so the clock must move for it to stop.
    time_->step_ms = 1;
    scheduler_ = std::make_unique<MaintenanceScheduler>(std::move(time));
  }

  FakeTask* AddTask(const std::string& name) {
    auto task = std::make_unique<FakeTask>(name, turns_);
    FakeTask* raw = task.get();
    scheduler_->Add(std::move(task));
    return raw;
  }

  FakeTime* time_ = nullptr;
  std::unique_ptr<MaintenanceScheduler> scheduler_;
  std::vector<std::string> turns_;
};

}  // namespace

TEST_F(MaintenanceSchedulerTest, NothingPendingWithoutTasks) {
  EXPECT_FALSE(scheduler_->RunSlice(1));
  EXPECT_FALSE(scheduler_->HasPendingWork());
}

TEST_F(MaintenanceSchedulerTest, PendingReflectsEachTasksLastTurn) {
  FakeTask* first = AddTask("a");
  FakeTask* second = AddTask("b");

  EXPECT_FALSE(scheduler_->HasPendingWork());

  second->pending = true;
  EXPECT_TRUE(scheduler_->RunSlice(1));

  second->pending = false;
  first->pending = false;
  EXPECT_FALSE(scheduler_->RunSlice(1));
}

TEST_F(MaintenanceSchedulerTest, HasTimeEndsWhenBudgetIsSpent) {
  constexpr std::int64_t kBudgetMs = 5;
  FakeTask* task = AddTask("a");

  // The scheduler reads the start time, then each check advances the clock
  // 1ms: checks at +1..+4 are within a 5ms budget, +5 is not.
  scheduler_->RunSlice(kBudgetMs);
  EXPECT_EQ(task->time_checks, 4);
}

TEST_F(MaintenanceSchedulerTest, EveryTaskGetsATurnAndTheFirstRotates) {
  AddTask("a");
  AddTask("b");
  AddTask("c");

  scheduler_->RunSlice(1);
  scheduler_->RunSlice(1);

  const std::vector<std::string> expected = {"a", "b", "c", "b", "c", "a"};
  EXPECT_EQ(turns_, expected);
}
