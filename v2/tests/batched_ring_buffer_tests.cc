#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <thread>
#include <vector>

#include "concurrent/batched_ring_buffer.h"

namespace myredis {
namespace {

// A page of ints, so the buffer has exactly this many slots and a batch
// straddling the end has to be read through the mirror mapping.
constexpr std::size_t kPageOfInts = 4096 / sizeof(int);

TEST(BatchRingBufferTest, PopsElementsInPushOrder) {
  constexpr std::size_t kCapacity = 8;
  constexpr int kPushed = 5;
  BatchRingBuffer<int, kCapacity> buffer;
  for (int value = 0; value < kPushed; ++value) {
    ASSERT_TRUE(buffer.Push(value));
  }
  EXPECT_EQ(buffer.Size(), kPushed);

  {
    auto batch = buffer.Pop(3);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(std::vector<int>(batch->begin(), batch->end()),
              (std::vector<int>{0, 1, 2}));
  }
  EXPECT_EQ(buffer.Size(), 2);

  auto rest = buffer.Pop(2);
  ASSERT_TRUE(rest.has_value());
  EXPECT_EQ((*rest)[0], 3);
  EXPECT_EQ((*rest)[1], 4);
  EXPECT_THROW((void)(*rest)[2], std::out_of_range);
}

TEST(BatchRingBufferTest, PopReturnsNulloptWhenTooFewElements) {
  constexpr std::size_t kCapacity = 8;
  BatchRingBuffer<int, kCapacity> buffer;
  ASSERT_TRUE(buffer.Push(1));
  EXPECT_FALSE(buffer.Pop(2).has_value());
  EXPECT_EQ(buffer.Size(), 1);
}

TEST(BatchRingBufferTest, PushFailsWhenFull) {
  BatchRingBuffer<int, 4> buffer;
  for (int value = 0; value < 4; ++value) ASSERT_TRUE(buffer.Push(value));
  EXPECT_FALSE(buffer.Push(4));
  {
    auto batch = buffer.Pop(1);
  }
  EXPECT_TRUE(buffer.Push(4));
}

TEST(BatchRingBufferTest, PopRejectsOversizedAndOverlappingPops) {
  constexpr std::size_t kCapacity = 4;
  BatchRingBuffer<int, kCapacity> buffer;
  ASSERT_TRUE(buffer.Push(1));
  ASSERT_TRUE(buffer.Push(2));
  EXPECT_THROW((void)buffer.Pop(kCapacity + 1), std::out_of_range);

  auto batch = buffer.Pop(1);
  ASSERT_TRUE(batch.has_value());
  EXPECT_THROW((void)buffer.Pop(1), std::logic_error);
}

TEST(BatchRingBufferTest, BatchIsContiguousAcrossTheWrap) {
  constexpr int kRounds = 10;
  // Not a divisor of the capacity, so batches land on every offset.
  constexpr std::size_t kLeftBehind = 7;
  BatchRingBuffer<int, kPageOfInts> buffer;

  int next_push = 0;
  int next_pop = 0;
  for (int round = 0; round < kRounds; ++round) {
    while (buffer.Push(next_push)) ++next_push;
    auto batch = buffer.Pop(kPageOfInts - kLeftBehind);
    ASSERT_TRUE(batch.has_value());
    for (const int value : *batch) EXPECT_EQ(value, next_pop++);
  }
}

TEST(BatchRingBufferTest, CapacityNotAPageMultiple) {
  // 12-byte elements: 1000 of them round up to lcm(4096, 12) = 12288 bytes, so
  // the buffer has 1024 slots but must still stop at 1000 elements.
  struct Triple {
    int first;
    int second;
    int third;
  };
  constexpr std::size_t kCapacity = 1000;
  constexpr std::size_t kBatchSize = 7;
  constexpr int kRounds = 1000;
  BatchRingBuffer<Triple, kCapacity> buffer;

  int next_push = 0;
  int next_pop = 0;
  for (int round = 0; round < kRounds; ++round) {
    while (buffer.Push(
        {.first = next_push, .second = -next_push, .third = next_push * 2})) {
      ++next_push;
    }
    EXPECT_EQ(buffer.Size(), kCapacity);
    auto batch = buffer.Pop(kBatchSize);
    ASSERT_TRUE(batch.has_value());
    for (const Triple& triple : *batch) {
      EXPECT_EQ(triple.first, next_pop);
      EXPECT_EQ(triple.second, -next_pop);
      EXPECT_EQ(triple.third, next_pop * 2);
      ++next_pop;
    }
  }
}

TEST(BatchRingBufferTest, ReserveAndCommitPublishOnlyCommittedSlots) {
  constexpr std::size_t kCapacity = 8;
  constexpr int kWritten = 5;
  BatchRingBuffer<int, kCapacity> buffer;
  ASSERT_TRUE(buffer.Push(-1));

  std::span<int> slots = buffer.Reserve(kCapacity * 2);
  ASSERT_EQ(slots.size(), kCapacity - 1);
  for (int index = 0; index < kWritten; ++index) slots[index] = index;
  EXPECT_EQ(buffer.Size(), 1);
  buffer.Commit(kWritten);
  EXPECT_EQ(buffer.Size(), kWritten + 1);

  auto batch = buffer.Pop(kWritten + 1);
  ASSERT_TRUE(batch.has_value());
  EXPECT_EQ(std::vector<int>(batch->begin(), batch->end()),
            (std::vector<int>{-1, 0, 1, 2, 3, 4}));
}

TEST(BatchRingBufferTest, ReserveIsEmptyWhenFull) {
  BatchRingBuffer<int, 2> buffer;
  ASSERT_TRUE(buffer.Push(1));
  ASSERT_TRUE(buffer.Push(2));
  EXPECT_TRUE(buffer.Reserve(1).empty());
  buffer.Commit(0);
  EXPECT_EQ(buffer.Size(), 2);
}

TEST(BatchRingBufferTest, ReserveRejectsMisuse) {
  BatchRingBuffer<int, 4> buffer;
  EXPECT_THROW(buffer.Commit(0), std::logic_error);

  (void)buffer.Reserve(2);
  EXPECT_THROW((void)buffer.Reserve(1), std::logic_error);
  EXPECT_THROW((void)buffer.Push(1), std::logic_error);
  EXPECT_THROW(buffer.Commit(3), std::out_of_range);
  buffer.Commit(2);
  EXPECT_TRUE(buffer.Push(1));
}

TEST(BatchRingBufferTest, ReservedSlotsAreContiguousAcrossTheWrap) {
  constexpr std::size_t kChunk = 100;
  constexpr int kRounds = 50;
  BatchRingBuffer<int, kPageOfInts> buffer;

  int next_write = 0;
  int next_pop = 0;
  for (int round = 0; round < kRounds; ++round) {
    std::span<int> slots = buffer.Reserve(kChunk);
    for (int& slot : slots) slot = next_write++;
    buffer.Commit(slots.size());

    auto batch = buffer.Pop(buffer.Size());
    ASSERT_TRUE(batch.has_value());
    for (const int value : *batch) EXPECT_EQ(value, next_pop++);
  }
  EXPECT_EQ(next_pop, kRounds * kChunk);
}

TEST(BatchRingBufferTest, ProducerAndConsumerOnSeparateThreads) {
  constexpr std::size_t kBatchSize = 7;
  constexpr std::size_t kCapacity = 64;
  constexpr long kBatches = 100'000;
  constexpr long kTotal = kBatches * kBatchSize;
  BatchRingBuffer<long, kCapacity> buffer;

  std::thread producer([&buffer] {
    for (long value = 0; value < kTotal; ++value) {
      while (!buffer.Push(value)) std::this_thread::yield();
    }
  });

  long next_pop = 0;
  while (next_pop < kTotal) {
    auto batch = buffer.Pop(kBatchSize);
    if (!batch) {
      std::this_thread::yield();
      continue;
    }
    for (const long value : *batch) ASSERT_EQ(value, next_pop++);
  }
  producer.join();
  EXPECT_EQ(buffer.Size(), 0);
}

TEST(BatchRingBufferTest, ReservingProducerAndConsumerOnSeparateThreads) {
  constexpr std::size_t kBatchSize = 7;
  static constexpr std::size_t kChunk = 10;
  constexpr std::size_t kCapacity = 64;
  constexpr long kBatches = 100'000;
  constexpr long kTotal = kBatches * kBatchSize;
  BatchRingBuffer<long, kCapacity> buffer;

  std::thread producer([&buffer] {
    long next_write = 0;
    while (next_write < kTotal) {
      std::span<long> slots =
          buffer.Reserve(std::min<std::size_t>(kChunk, kTotal - next_write));
      for (long& slot : slots) slot = next_write++;
      buffer.Commit(slots.size());
      if (slots.empty()) std::this_thread::yield();
    }
  });

  long next_pop = 0;
  while (next_pop < kTotal) {
    auto batch = buffer.Pop(kBatchSize);
    if (!batch) {
      std::this_thread::yield();
      continue;
    }
    for (const long value : *batch) ASSERT_EQ(value, next_pop++);
  }
  producer.join();
  EXPECT_EQ(buffer.Size(), 0);
}

}  // namespace
}  // namespace myredis
