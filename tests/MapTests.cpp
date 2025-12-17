#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "store/Hash.h"
#include "store/LinearProbingHashmap.h"
#include "store/LinkedListHashmap.h"
#include "store/StandardMap.h"
#include "store/StripedHashmap.h"

struct LinearProbingHashmapStringIntFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    // Use small initial capacity to make resize easier to trigger in tests
    return std::make_unique<LinearProbingHashmap<std::string, int>>(
        DEFAULT_LOAD_FACTOR, string_hash, 2);
  }
};

struct LinkedListHashmapStringIntFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    return std::make_unique<LinkedListHashmap<std::string, int>>(
        DEFAULT_LOAD_FACTOR, string_hash);
  }
};

struct StandardMapFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    return std::make_unique<StandardMap<std::string, int>>();
  }
};

struct StripedHashmapFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    return std::make_unique<StripedHashmap<std::string, int>>(
        DEFAULT_LOAD_FACTOR, string_hash);
  }
};

// 1. Template the test fixture on a type `T`
template <typename MapFactory>
class MapTest : public ::testing::Test {
 protected:
  void SetUp() override { map = MapFactory::create(); }

  std::unique_ptr<Map<std::string, int>> map;
};

// List of types to be tested
using Implementations =
    ::testing::Types<LinearProbingHashmapStringIntFactory, StandardMapFactory,
                     LinkedListHashmapStringIntFactory, StripedHashmapFactory>;

TYPED_TEST_SUITE(MapTest, Implementations);

TYPED_TEST(MapTest, InsertAndLookUp) {
  this->map->Insert(std::string("one"), 1);
  auto opt = this->map->LookUp(std::string("one"));
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->get(), 1);
}

TYPED_TEST(MapTest, LookUpNonExistent) {
  auto opt = this->map->LookUp(std::string("nonexistent"));
  EXPECT_FALSE(opt.has_value());
}

TYPED_TEST(MapTest, InsertRemoveLookUp) {
  this->map->Insert(std::string("one"), 1);
  this->map->Remove(std::string("one"));
  EXPECT_FALSE(this->map->LookUp(std::string("one")).has_value());
}

TYPED_TEST(MapTest, RemoveNonExistent) {
  ASSERT_NO_THROW(this->map->Remove(std::string("nonexistent")));
}

TYPED_TEST(MapTest, UpdateValue) {
  this->map->Insert(std::string("one"), 1);
  constexpr int kUpdatedValue = 100;
  this->map->Insert(std::string("one"), kUpdatedValue);
  auto opt = this->map->LookUp(std::string("one"));
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->get(), kUpdatedValue);
}

TYPED_TEST(MapTest, MultipleInsertions) {
  this->map->Insert(std::string("one"), 1);
  this->map->Insert(std::string("two"), 2);
  this->map->Insert(std::string("three"), 3);

  const auto value_one = this->map->LookUp(std::string("one"));
  ASSERT_TRUE(value_one.has_value());
  EXPECT_EQ(value_one->get(), 1);

  const auto value_two = this->map->LookUp(std::string("two"));
  ASSERT_TRUE(value_two.has_value());
  EXPECT_EQ(value_two->get(), 2);

  const auto value_three = this->map->LookUp(std::string("three"));
  ASSERT_TRUE(value_three.has_value());
  EXPECT_EQ(value_three->get(), 3);
}

TYPED_TEST(MapTest, OperationsAreIsolated) {
  this->map->Insert(std::string("one"), 1);
  this->map->Insert(std::string("two"), 2);

  this->map->Remove(std::string("one"));
  EXPECT_FALSE(this->map->LookUp(std::string("one")).has_value());

  const auto value_two = this->map->LookUp(std::string("two"));
  ASSERT_TRUE(value_two.has_value());
  EXPECT_EQ(value_two->get(), 2);
}

// Performance-based tests: do not rely on internals. These tests will fail
// if inserting or looking up many elements is too slow. Thresholds are
// intentionally generous but will catch extremely slow implementations.
TYPED_TEST(MapTest, BulkInsertPerformance) {
  // Skip very small runs for StandardMap as its behaviour is fine; we still
  // want to run the test for all implementations.
  constexpr size_t kNumElements = 10000;
  using clock = std::chrono::high_resolution_clock;

  const auto start = clock::now();
  for (size_t i = 0; i < kNumElements; ++i) {
    this->map->Insert(std::to_string(i), static_cast<int>(i));
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      clock::now() - start);

  // If inserting kNumElements elements takes longer than 200 ms, consider it a
  // failure.
  EXPECT_LT(duration.count(), 200)
      << "Bulk insert too slow: " << duration.count() << " ms";
}

TYPED_TEST(MapTest, BulkLookupPerformance) {
  constexpr size_t kNumElements = 10000;
  using clock = std::chrono::high_resolution_clock;

  // Ensure the map is populated.
  for (size_t i = 0; i < kNumElements; ++i) {
    this->map->Insert(std::to_string(i), static_cast<int>(i));
  }

  auto start = clock::now();
  for (size_t i = 0; i < kNumElements; ++i) {
    const auto value = this->map->LookUp(std::to_string(i));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->get(), static_cast<int>(i));
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      clock::now() - start);

  // If looking up kNumElements elements takes longer than 200 ms, consider it a
  // failure.
  EXPECT_LT(duration.count(), 200)
      << "Bulk lookup too slow: " << duration.count() << " ms";
}

TYPED_TEST(MapTest, BulkRemovePerformance) {
  constexpr size_t kNumElements = 10000;
  using clock = std::chrono::high_resolution_clock;

  // Ensure the map is populated.
  for (size_t i = 0; i < kNumElements; ++i) {
    this->map->Insert(std::to_string(i), static_cast<int>(i));
  }

  auto start = clock::now();
  for (size_t i = 0; i < kNumElements; ++i) {
    this->map->Remove(std::to_string(i));
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      clock::now() - start);

  // If removing kNumElements elements takes longer than 200 ms, consider it a
  // failure.
  EXPECT_LT(duration.count(), 200)
      << "Bulk remove too slow: " << duration.count() << " ms";
}

// --- Benchmarking Tests ---
// These tests are not for correctness but for performance comparison.
// They will output the timings of different implementations for bulk
// operations.
TEST(MapBenchmark, CompareImplementations) {
  constexpr size_t kBenchmarkSize = 50000;
  using clock = std::chrono::high_resolution_clock;

  std::map<std::string, std::function<std::unique_ptr<Map<std::string, int>>()>>
      factories;
  factories["StandardMap"] = &StandardMapFactory::create;
  factories["LinkedListHashmap"] = &LinkedListHashmapStringIntFactory::create;
  factories["LinearProbingHashmap"] =
      &LinearProbingHashmapStringIntFactory::create;
  factories["StripedHashmap"] = &StripedHashmapFactory::create;

  std::cout << "\n"
            << "[==========] Running MapBenchmark for N = " << kBenchmarkSize
            << " operations." << "\n";

  // --- Insert benchmark ---
  std::cout << "[ BENCHMARK ] Bulk Insert" << "\n";
  for (auto const& [name, factory] : factories) {
    auto map = factory();
    const auto start = clock::now();
    for (size_t i = 0; i < kBenchmarkSize; ++i) {
      map->Insert(std::to_string(i), static_cast<int>(i));
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - start);
    std::cout << "[ RESULT    ] " << name << ": " << duration.count() << " ms"
              << "\n";
  }

  // --- Lookup benchmark ---
  std::cout << "[ BENCHMARK ] Bulk Lookup" << "\n";
  for (auto const& [name, factory] : factories) {
    auto map = factory();
    for (size_t i = 0; i < kBenchmarkSize; ++i) {
      map->Insert(std::to_string(i), static_cast<int>(i));
    }
    const auto start = clock::now();
    for (size_t i = 0; i < kBenchmarkSize; ++i) {
      map->LookUp(std::to_string(i));
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - start);
    std::cout << "[ RESULT    ] " << name << ": " << duration.count() << " ms"
              << "\n";
  }

  // --- Remove benchmark ---
  std::cout << "[ BENCHMARK ] Bulk Remove" << "\n";
  for (auto const& [name, factory] : factories) {
    auto map = factory();
    for (size_t i = 0; i < kBenchmarkSize; ++i) {
      map->Insert(std::to_string(i), static_cast<int>(i));
    }
    const auto start = clock::now();
    for (size_t i = 0; i < kBenchmarkSize; ++i) {
      map->Remove(std::to_string(i));
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - start);
    std::cout << "[ RESULT    ] " << name << ": " << duration.count() << " ms"
              << "\n";
  }
  std::cout << "[==========] Finished MapBenchmark." << "\n";
}

// End suppression for magic-number warnings
// NOLINTEND(readability-magic-numbers)