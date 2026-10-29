

#ifndef MYREDIS_DATA_STRUCTURES_MAP_LINEAR_PROBING_HASHMAP_H_
#define MYREDIS_DATA_STRUCTURES_MAP_LINEAR_PROBING_HASHMAP_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "data_structures/map/map.h"

namespace myredis {

template <typename K, typename V>
class LinearProbingHashmap final : public Map<K, V> {
  enum State : std::uint8_t { EMPTY, DELETED, ELEMENT };
  struct Entry {
    Entry() : state(EMPTY), key(std::nullopt), value(std::nullopt) {}
    Entry(const State state, const std::optional<K>& key,
          const std::optional<V>& value)
        : state(state), key(key), value(value) {}
    State state;
    std::optional<K> key;
    std::optional<V> value;
  };

  std::function<size_t(const K&)> hash_;
  double load_factor_;
  std::vector<Entry> entries_;
  size_t size_ = 0;
  // DELETED slots. They lengthen probe sequences like live elements do, so
  // they count towards the load factor until a resize clears them.
  size_t tombstones_ = 0;

 public:
  LinearProbingHashmap(const LinearProbingHashmap& other)
    requires std::is_copy_constructible_v<K> && std::is_copy_constructible_v<V>
      : Map<K, V>(other),
        hash_(other.hash_),
        load_factor_(other.load_factor_),
        entries_(other.entries_),
        size_(other.size_),
        tombstones_(other.tombstones_) {}

  LinearProbingHashmap(LinearProbingHashmap&& other) noexcept
      : Map<K, V>(std::move(other)),
        hash_(std::move(other.hash_)),
        load_factor_(other.load_factor_),
        entries_(std::move(other.entries_)),
        size_(other.size_),
        tombstones_(other.tombstones_) {}

  LinearProbingHashmap& operator=(const LinearProbingHashmap& other)
    requires std::is_copy_constructible_v<K> && std::is_copy_constructible_v<V>
  {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    hash_ = other.hash_;
    load_factor_ = other.load_factor_;
    entries_ = other.entries_;
    size_ = other.size_;
    tombstones_ = other.tombstones_;
    return *this;
  }

  LinearProbingHashmap& operator=(LinearProbingHashmap&& other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    hash_ = std::move(other.hash_);
    load_factor_ = other.load_factor_;
    entries_ = std::move(other.entries_);
    size_ = other.size_;
    tombstones_ = other.tombstones_;
    return *this;
  }

  LinearProbingHashmap(const double load_factor,
                       std::function<size_t(const K&)> hash,
                       const size_t initial_capacity = kDefaultCapacity) {
    this->hash_ = std::move(hash);
    this->load_factor_ = load_factor;
    this->entries_ = std::vector<Entry>(initial_capacity);
  }

  std::optional<std::reference_wrapper<V>> LookUp(const K& key) override {
    const int value_bucket_index = InternalFind(key);

    if (value_bucket_index == -1) return std::nullopt;

    std::optional<V>& value = entries_[value_bucket_index].value;
    assert(value.has_value());
    if (!value.has_value()) return std::nullopt;
    return std::optional<std::reference_wrapper<V>>(std::ref(*value));
  }

  void Insert(K key, V value) override {
    // Look for the key across the whole probe sequence first: it may sit past
    // a tombstone, and inserting at that tombstone would duplicate it.
    const int existing_index = InternalFind(key);
    if (existing_index != -1) {
      entries_[existing_index].value = std::move(value);
      return;
    }

    if (NeedsResizeForInsert()) Resize();
    InsertNew(std::move(key), std::move(value));
  }

  void Remove(const K& key) override {
    const int bucket_index = InternalFind(key);
    if (bucket_index != -1) {
      Entry& entry = entries_[bucket_index];
      entry.state = DELETED;
      // Free the removed key and value now rather than when the slot is reused
      entry.key.reset();
      entry.value.reset();
      size_--;
      tombstones_++;
    }
  }

  void ForEach(std::function<void(const K&, V&)> action) override {
    for (Entry& entry : entries_) {
      assert(entry.state != ELEMENT ||
             (entry.key.has_value() && entry.value.has_value()));
      if (entry.state == ELEMENT && entry.key.has_value() &&
          entry.value.has_value()) {
        action(*entry.key, *entry.value);
      }
    }
  }

  std::optional<K> RandomKey(std::mt19937_64& rng) override {
    if (entries_.empty()) return std::nullopt;
    // Start at a random slot and scan forward (wrapping) to the first live
    // element. Elements that follow runs of empty/deleted slots are favoured
    // slightly, which is fine for sampling.
    std::uniform_int_distribution<size_t> pick_slot(0, entries_.size() - 1);
    const size_t start = pick_slot(rng);
    for (size_t offset = 0; offset < entries_.size(); ++offset) {
      const Entry& entry = entries_[(start + offset) % entries_.size()];
      if (entry.state == ELEMENT && entry.key.has_value()) return *entry.key;
    }
    return std::nullopt;
  }

 private:
  // True if inserting one more key would push the occupied slots (live
  // elements plus tombstones, since both lengthen probe sequences) past the
  // load factor.
  [[nodiscard]] bool NeedsResizeForInsert() const {
    return static_cast<double>(size_ + tombstones_ + 1) >
           load_factor_ * static_cast<double>(entries_.size());
  }

  // Places a key known not to be in the map into the first free (empty or
  // deleted) slot of its probe sequence. Does not resize.
  void InsertNew(K key, V value) {
    size_t bucket_index = hash_(key) % entries_.size();
    while (entries_[bucket_index].state == ELEMENT) {
      bucket_index = (bucket_index + 1) % entries_.size();
    }

    Entry& entry = entries_[bucket_index];
    if (entry.state == DELETED) tombstones_--;
    entry.state = ELEMENT;
    entry.key = std::move(key);
    entry.value = std::move(value);
    size_++;
  }

  // Rebuilds the table, dropping tombstones. Capacity comes from the live
  // count (plus the insert that triggered this) rather than the old capacity,
  // so a table that filled up mostly with tombstones does not keep growing.
  void Resize() {
    std::vector<Entry> old_entries = std::move(entries_);

    entries_ =
        std::vector<Entry>(std::max(static_cast<size_t>(2), (size_ + 1) * 2));
    size_ = 0;
    tombstones_ = 0;

    for (Entry& entry : old_entries) {
      assert(entry.state != ELEMENT ||
             (entry.key.has_value() && entry.value.has_value()));
      if (entry.state == ELEMENT && entry.key.has_value() &&
          entry.value.has_value()) {
        InsertNew(std::move(*entry.key), std::move(*entry.value));
      }
    }
  }

  // True if `entry` is occupied by a key other than `key`, i.e. probing must
  // continue past it.
  static bool HoldsOtherKey(const Entry& entry, const K& key) {
    return entry.state == ELEMENT && entry.key.has_value() && *entry.key != key;
  }

  // Returns -1 if key cannot be found
  int InternalFind(const K& key) {
    size_t bucket_index = hash_(key) % entries_.size();
    const size_t initial_bucket_index = bucket_index;

    while (entries_[bucket_index].state == DELETED ||
           HoldsOtherKey(entries_[bucket_index], key)) {
      bucket_index = (bucket_index + 1) % entries_.size();
      if (bucket_index == initial_bucket_index) {
        // Gone through every element in the hashmap and could not find the key
        return -1;
      }
    }

    if (entries_[bucket_index].state == ELEMENT) {
      return static_cast<int>(bucket_index);
    }
    return -1;
  }
};

}  // namespace myredis

#endif  // MYREDIS_DATA_STRUCTURES_MAP_LINEAR_PROBING_HASHMAP_H_
