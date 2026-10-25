#ifndef MYREDIS_SERVER_HANDLER_COMMAND_TABLE_H_
#define MYREDIS_SERVER_HANDLER_COMMAND_TABLE_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace myredis {

// Compile-time perfect hash from N command names to their indices [0, N).
// The seed is searched for at compile time so that every name lands in its own
// slot; a lookup is then one hash, one table load and one string compare.
//
// Must be constructed in a constant expression: duplicate names, or failing to
// find a seed, are compile errors.
template <std::size_t N>
class CommandTable {
 public:
  // Returned by Find for names not in the table.
  static constexpr std::size_t kNotFound = N;

  consteval explicit CommandTable(const std::array<std::string_view, N>& names)
      : seed_(FindSeed(names)) {
    indices_.fill(kNotFound);
    for (std::size_t i = 0; i < N; ++i) {
      const std::size_t slot = SlotOf(names[i], seed_);
      names_[slot] = names[i];
      indices_[slot] = i;
    }
  }

  static consteval bool AreUnique(
      const std::array<std::string_view, N>& names) {
    for (std::size_t i = 0; i < N; ++i) {
      for (std::size_t j = i + 1; j < N; ++j) {
        if (names[i] == names[j]) return false;
      }
    }
    return true;
  }

  // Returns the index of the (case-sensitive) `name`, or kNotFound.
  [[nodiscard]] constexpr std::size_t Find(std::string_view name) const {
    const std::size_t slot = SlotOf(name, seed_);
    return names_[slot] == name ? indices_[slot] : kNotFound;
  }

 private:
  // Twice the name count, rounded up to a power of two, keeps the seed search
  // short and lets the slot be taken with a shift instead of a modulo.
  static constexpr std::size_t kSlotCount =
      std::bit_ceil(std::max<std::size_t>(N * 2, 2));
  static constexpr int kSlotShift = 32 - std::countr_zero(kSlotCount);

  // Seeded FNV-1a.
  static constexpr std::uint32_t Hash(std::string_view name,
                                      std::uint32_t seed) {
    constexpr std::uint32_t kFnvOffset = 2166136261U;
    constexpr std::uint32_t kFnvPrime = 16777619U;
    std::uint32_t hash = kFnvOffset ^ seed;
    for (const char character : name) {
      hash ^= static_cast<unsigned char>(character);
      hash *= kFnvPrime;
    }
    return hash;
  }

  // Uses the top bits: FNV's low bits only depend on the seed's low bits, so
  // masking them would leave most seeds producing identical layouts.
  static constexpr std::size_t SlotOf(std::string_view name,
                                      std::uint32_t seed) {
    return Hash(name, seed) >> kSlotShift;
  }

  static consteval bool IsPerfect(const std::array<std::string_view, N>& names,
                                  std::uint32_t seed) {
    std::array<bool, kSlotCount> used{};
    for (const std::string_view name : names) {
      const std::size_t slot = SlotOf(name, seed);
      if (used[slot]) return false;
      used[slot] = true;
    }
    return true;
  }

  static consteval std::uint32_t FindSeed(
      const std::array<std::string_view, N>& names) {
    // Reaching a throw in a constant expression is a compile error.
    if (!AreUnique(names)) throw std::logic_error("duplicate command name");
    constexpr std::uint32_t kMaxSeed = 1U << 16U;
    for (std::uint32_t seed = 0; seed < kMaxSeed; ++seed) {
      if (IsPerfect(names, seed)) return seed;
    }
    throw std::logic_error("no perfect hash seed for these command names");
  }

  std::uint32_t seed_;
  std::array<std::string_view, kSlotCount> names_{};
  std::array<std::size_t, kSlotCount> indices_{};
};

namespace command_table_internal {

inline constexpr CommandTable<3> kExample({"GET", "SET", "DEL"});
static_assert(kExample.Find("GET") == 0);
static_assert(kExample.Find("SET") == 1);
static_assert(kExample.Find("DEL") == 2);
static_assert(kExample.Find("get") == decltype(kExample)::kNotFound);
static_assert(kExample.Find("") == decltype(kExample)::kNotFound);

}  // namespace command_table_internal

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_COMMAND_TABLE_H_
