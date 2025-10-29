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

struct LinearProbingHashmapStringIntFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    // Use small initial capacity to make resize easier to trigger in tests
    return std::make_unique<LinearProbingHashmap<std::string, int>>(
        0.75, string_hash, 2);
  }
};

struct LinkedListHashmapStringIntFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    return std::make_unique<LinkedListHashmap<std::string, int>>(0.75,
                                                                 string_hash);
  }
};

struct StandardMapFactory {
  static std::unique_ptr<Map<std::string, int>> create() {
    return std::make_unique<StandardMap<std::string, int>>();
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
                     LinkedListHashmapStringIntFactory>;

TYPED_TEST_SUITE(MapTest, Implementations);

TYPED_TEST(MapTest, InsertAndLookUp) {
  this->map->Insert("one", 1);
  const std::unique_ptr<int> value = this->map->LookUp("one");
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 1);
}

TYPED_TEST(MapTest, LookUpNonExistent) {
  const std::unique_ptr<int> value = this->map->LookUp("nonexistent");
  EXPECT_EQ(value, nullptr);
}

TYPED_TEST(MapTest, InsertRemoveLookUp) {
  this->map->Insert("one", 1);
  this->map->Remove("one");
  const std::unique_ptr<int> value = this->map->LookUp("one");
  EXPECT_EQ(value, nullptr);
}

TYPED_TEST(MapTest, RemoveNonExistent) {
  ASSERT_NO_THROW(this->map->Remove("nonexistent"));
}

TYPED_TEST(MapTest, UpdateValue) {
  this->map->Insert("one", 1);
  this->map->Insert("one", 100);
  const std::unique_ptr<int> value = this->map->LookUp("one");
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 100);
}

TYPED_TEST(MapTest, MultipleInsertions) {
  this->map->Insert("one", 1);
  this->map->Insert("two", 2);
  this->map->Insert("three", 3);

  const std::unique_ptr<int> value_one = this->map->LookUp("one");
  ASSERT_NE(value_one, nullptr);
  EXPECT_EQ(*value_one, 1);

  const std::unique_ptr<int> value_two = this->map->LookUp("two");
  ASSERT_NE(value_two, nullptr);
  EXPECT_EQ(*value_two, 2);

  const std::unique_ptr<int> value_three = this->map->LookUp("three");
  ASSERT_NE(value_three, nullptr);
  EXPECT_EQ(*value_three, 3);
}

TYPED_TEST(MapTest, OperationsAreIsolated) {
  this->map->Insert("one", 1);
  this->map->Insert("two", 2);

  this->map->Remove("one");

  EXPECT_EQ(this->map->LookUp("one"), nullptr);

  const std::unique_ptr<int> value_two = this->map->LookUp("two");
  ASSERT_NE(value_two, nullptr);
  EXPECT_EQ(*value_two, 2);
}

// Performance-based tests: do not rely on internals. These tests will fail
// if inserting or looking up many elements is too slow. Thresholds are
// intentionally generous but will catch extremely slow implementations.
TYPED_TEST(MapTest, BulkInsertPerformance) {
  // Skip very small runs for StandardMap as its behaviour is fine; we still
  // want to run the test for all implementations.
  constexpr size_t N = 10000;
  using clock = std::chrono::high_resolution_clock;

  const auto start = clock::now();
  for (size_t i = 0; i < N; ++i) {
    this->map->Insert(std::to_string(i), static_cast<int>(i));
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      clock::now() - start);

  // If inserting N elements takes longer than 200 ms, consider it a failure.
  EXPECT_LT(duration.count(), 200)
      << "Bulk insert too slow: " << duration.count() << " ms";
}

TYPED_TEST(MapTest, BulkLookupPerformance) {
  constexpr size_t N = 10000;
  using clock = std::chrono::high_resolution_clock;

  // Ensure the map is populated.
  for (size_t i = 0; i < N; ++i) {
    this->map->Insert(std::to_string(i), static_cast<int>(i));
  }

  auto start = clock::now();
  for (size_t i = 0; i < N; ++i) {
    const auto v = this->map->LookUp(std::to_string(i));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, static_cast<int>(i));
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      clock::now() - start);

  // If looking up N elements takes longer than 200 ms, consider it a failure.
  EXPECT_LT(duration.count(), 200)
      << "Bulk lookup too slow: " << duration.count() << " ms";
}

TYPED_TEST(MapTest, BulkRemovePerformance) {
  constexpr size_t N = 10000;
  using clock = std::chrono::high_resolution_clock;

  // Ensure the map is populated.
  for (size_t i = 0; i < N; ++i) {
    this->map->Insert(std::to_string(i), static_cast<int>(i));
  }

  auto start = clock::now();
  for (size_t i = 0; i < N; ++i) {
    this->map->Remove(std::to_string(i));
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      clock::now() - start);

  // If removing N elements takes longer than 200 ms, consider it a failure.
  EXPECT_LT(duration.count(), 200)
      << "Bulk remove too slow: " << duration.count() << " ms";
}

// --- Benchmarking Tests ---
// These tests are not for correctness but for performance comparison.
// They will output the timings of different implementations for bulk
// operations.
TEST(MapBenchmark, CompareImplementations) {
  constexpr size_t N = 50000;
  using clock = std::chrono::high_resolution_clock;

  std::map<std::string, std::function<std::unique_ptr<Map<std::string, int>>()>>
      factories;
  factories["StandardMap"] = &StandardMapFactory::create;
  factories["LinkedListHashmap"] = &LinkedListHashmapStringIntFactory::create;
  factories["LinearProbingHashmap"] =
      &LinearProbingHashmapStringIntFactory::create;

  std::cout << std::endl
            << "[==========] Running MapBenchmark for N = " << N
            << " operations." << std::endl;

  // --- Insert benchmark ---
  std::cout << "[ BENCHMARK ] Bulk Insert" << std::endl;
  for (auto const& [name, factory] : factories) {
    auto map = factory();
    const auto start = clock::now();
    for (size_t i = 0; i < N; ++i) {
      map->Insert(std::to_string(i), static_cast<int>(i));
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - start);
    std::cout << "[ RESULT    ] " << name << ": " << duration.count() << " ms"
              << std::endl;
  }

  // --- Lookup benchmark ---
  std::cout << "[ BENCHMARK ] Bulk Lookup" << std::endl;
  for (auto const& [name, factory] : factories) {
    auto map = factory();
    for (size_t i = 0; i < N; ++i) {
      map->Insert(std::to_string(i), static_cast<int>(i));
    }
    const auto start = clock::now();
    for (size_t i = 0; i < N; ++i) {
      map->LookUp(std::to_string(i));
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - start);
    std::cout << "[ RESULT    ] " << name << ": " << duration.count() << " ms"
              << std::endl;
  }

  // --- Remove benchmark ---
  std::cout << "[ BENCHMARK ] Bulk Remove" << std::endl;
  for (auto const& [name, factory] : factories) {
    auto map = factory();
    for (size_t i = 0; i < N; ++i) {
      map->Insert(std::to_string(i), static_cast<int>(i));
    }
    const auto start = clock::now();
    for (size_t i = 0; i < N; ++i) {
      map->Remove(std::to_string(i));
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - start);
    std::cout << "[ RESULT    ] " << name << ": " << duration.count() << " ms"
              << std::endl;
  }
  std::cout << "[==========] Finished MapBenchmark." << std::endl;
}