#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "store/store.h"
#include "time/time.h"

using myredis::Store;
using myredis::Time;

namespace {

constexpr std::uint64_t kSeed = 42;
constexpr std::int64_t kStartMs = 1000;
constexpr std::int64_t kTtlMs = 500;

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

class StoreExpiryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto time = std::make_unique<FakeTime>();
    time_ = time.get();
    store_ = std::make_unique<Store>(std::move(time), kSeed);
  }

  void SetWithTtl(const std::string& key) {
    store_->Set(key, "value");
    ASSERT_TRUE(store_->ExpireAt(key, kStartMs + kTtlMs));
  }

  void AdvancePastTtl() { time_->now_ms = kStartMs + kTtlMs + 1; }

  FakeTime* time_ = nullptr;
  std::unique_ptr<Store> store_;
};

}  // namespace

TEST_F(StoreExpiryTest, EmptyStoreRemovesNothing) {
  EXPECT_EQ(store_->ActiveExpireCycle(), 0);
}

TEST_F(StoreExpiryTest, RemovesExpiredKey) {
  SetWithTtl("a");
  AdvancePastTtl();

  EXPECT_EQ(store_->ActiveExpireCycle(), 1);
  EXPECT_EQ(store_->SerialiseToJson(), "{}");
}

TEST_F(StoreExpiryTest, KeepsKeysThatHaveNotExpired) {
  SetWithTtl("volatile");
  store_->Set("persistent", "value");

  EXPECT_EQ(store_->ActiveExpireCycle(), 0);
  EXPECT_EQ(store_->Get("volatile"), "value");
  EXPECT_EQ(store_->Get("persistent"), "value");
}

TEST_F(StoreExpiryTest, KeyExpiringExactlyNowIsKept) {
  SetWithTtl("a");
  time_->now_ms = kStartMs + kTtlMs;

  EXPECT_EQ(store_->ActiveExpireCycle(), 0);
  EXPECT_EQ(store_->Get("a"), "value");
}

TEST_F(StoreExpiryTest, RemovesEveryKeyWhenAllHaveExpired) {
  // Every sample is fully expired, so the cycle keeps sampling until the
  // index is empty (the fake clock never exhausts the time budget).
  constexpr int kNumKeys = 1000;
  for (int i = 0; i < kNumKeys; ++i) SetWithTtl(std::to_string(i));
  AdvancePastTtl();

  EXPECT_EQ(store_->ActiveExpireCycle(), kNumKeys);
  EXPECT_EQ(store_->SerialiseToJson(), "{}");
}

TEST_F(StoreExpiryTest, PersistedKeyIsNotRemoved) {
  SetWithTtl("a");
  ASSERT_TRUE(store_->Persist("a"));
  AdvancePastTtl();

  EXPECT_EQ(store_->ActiveExpireCycle(), 0);
  EXPECT_EQ(store_->Get("a"), "value");
}

TEST_F(StoreExpiryTest, OverwrittenKeyLosesItsTtl) {
  SetWithTtl("a");
  store_->Set("a", "new");
  AdvancePastTtl();

  EXPECT_EQ(store_->ActiveExpireCycle(), 0);
  EXPECT_EQ(store_->Get("a"), "new");
}

TEST_F(StoreExpiryTest, DeletedKeyLeavesTheIndex) {
  SetWithTtl("a");
  store_->Del("a");
  AdvancePastTtl();

  EXPECT_EQ(store_->ActiveExpireCycle(), 0);
}

TEST_F(StoreExpiryTest, RestoredTtlsAreExpired) {
  store_->DeserialiseFromJson(
      R"({"old":{"value":"v","expiry":1500},"kept":{"value":"v","expiry":-1}})");
  AdvancePastTtl();

  EXPECT_EQ(store_->ActiveExpireCycle(), 1);
  EXPECT_EQ(store_->SerialiseToJson(), R"({"kept":{"value":"v","expiry":-1}})");
}

TEST_F(StoreExpiryTest, StopsAfterOneRoundWhenPredicateDeclines) {
  constexpr int kNumKeys = 1000;
  for (int i = 0; i < kNumKeys; ++i) SetWithTtl(std::to_string(i));
  AdvancePastTtl();

  int rounds = 0;
  const std::size_t removed =
      store_->ActiveExpireCycle([&](const Store::ExpireRound& round) {
        rounds++;
        EXPECT_EQ(round.sampled, 20);
        EXPECT_EQ(round.removed, 20);
        return false;
      });

  EXPECT_EQ(rounds, 1);
  EXPECT_EQ(removed, 20);
}

TEST_F(StoreExpiryTest, StopsWhenIndexEmptiesEvenIfPredicateContinues) {
  SetWithTtl("a");
  SetWithTtl("b");
  AdvancePastTtl();

  EXPECT_EQ(
      store_->ActiveExpireCycle([](const Store::ExpireRound&) { return true; }),
      2);
}

TEST_F(StoreExpiryTest, DefaultPolicyStopsWhenTimeBudgetIsSpent) {
  // Every key has expired, so only the time budget can end the cycle early.
  // Each clock read jumps past the 25ms budget, so one round runs.
  constexpr int kNumKeys = 1000;
  for (int i = 0; i < kNumKeys; ++i) SetWithTtl(std::to_string(i));
  AdvancePastTtl();
  time_->step_ms = 30;

  EXPECT_EQ(store_->ActiveExpireCycle(), 20);
}
