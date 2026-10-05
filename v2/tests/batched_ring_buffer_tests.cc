#include "concurrent/batched_ring_buffer.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <thread>
#include <vector>

namespace myredis {
namespace {

TEST(BatchRingBufferTest, PopsElementsInPushOrder) {
  BatchRingBuffer<int, 8> buffer;
  for (int value = 0; value < 5; ++value) ASSERT_TRUE(buffer.Push(value));
  EXPECT_EQ(buffer.Size(), 5);

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
  BatchRingBuffer<int, 8> buffer;
  ASSERT_TRUE(buffer.Push(1));
  EXPECT_FALSE(buffer.Pop(2).has_value());
  EXPECT_EQ(buffer.Size(), 1);
}

TEST(BatchRingBufferTest, PushFailsWhenFull) {
  BatchRingBuffer<int, 4> buffer;
  for (int value = 0; value < 4; ++value) ASSERT_TRUE(buffer.Push(value));
  EXPECT_FALSE(buffer.Push(4));
  { auto batch = buffer.Pop(1); }
  EXPECT_TRUE(buffer.Push(4));
}

TEST(BatchRingBufferTest, PopRejectsOversizedAndOverlappingPops) {
  BatchRingBuffer<int, 4> buffer;
  ASSERT_TRUE(buffer.Push(1));
  ASSERT_TRUE(buffer.Push(2));
  EXPECT_THROW((void)buffer.Pop(5), std::out_of_range);

  auto batch = buffer.Pop(1);
  ASSERT_TRUE(batch.has_value());
  EXPECT_THROW((void)buffer.Pop(1), std::logic_error);
}

TEST(BatchRingBufferTest, BatchIsContiguousAcrossTheWrap) {
  // A page of ints, so the buffer has exactly kCapacity slots and a batch
  // straddling the end has to be read through the mirror mapping.
  constexpr std::size_t kCapacity = 4096 / sizeof(int);
  BatchRingBuffer<int, kCapacity> buffer;

  int next_push = 0;
  int next_pop = 0;
  for (int round = 0; round < 10; ++round) {
    while (buffer.Push(next_push)) ++next_push;
    auto batch = buffer.Pop(kCapacity - 7);
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
  BatchRingBuffer<Triple, kCapacity> buffer;

  int next_push = 0;
  int next_pop = 0;
  for (int round = 0; round < 1000; ++round) {
    while (buffer.Push({next_push, -next_push, next_push * 2})) ++next_push;
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

TEST(BatchRingBufferTest, ProducerAndConsumerOnSeparateThreads) {
  constexpr std::size_t kBatchSize = 7;
  constexpr std::size_t kCapacity = 64;
  constexpr long kTotal = 100'000 * kBatchSize;
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

}  // namespace
}  // namespace myredis
