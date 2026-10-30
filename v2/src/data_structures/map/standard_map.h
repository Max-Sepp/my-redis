#ifndef MYREDIS_DATA_STRUCTURES_MAP_STANDARD_MAP_H_
#define MYREDIS_DATA_STRUCTURES_MAP_STANDARD_MAP_H_

#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <random>
#include <unordered_map>
#include <utility>

#include "data_structures/map/map.h"

namespace myredis {

template <typename K, typename V>
class StandardMap final : public Map<K, V> {
 public:
  StandardMap() = default;

  StandardMap(const StandardMap& other)
      : Map<K, V>(other), data_(other.data_) {}
  StandardMap(StandardMap&& other) noexcept
      : Map<K, V>(std::move(other)), data_(std::move(other.data_)) {}
  StandardMap& operator=(const StandardMap& other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    data_ = other.data_;
    return *this;
  }
  StandardMap& operator=(StandardMap&& other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    data_ = std::move(other.data_);
    return *this;
  }

  std::optional<std::reference_wrapper<V>> LookUp(const K& key) override {
    const auto iter = data_.find(key);
    if (iter == data_.end()) return std::nullopt;
    return std::optional<std::reference_wrapper<V>>(std::ref(iter->second));
  }

  void Insert(K key, V value) override {
    data_.insert_or_assign(std::move(key), std::move(value));
  }

  void Remove(const K& key) override { data_.erase(key); }

  void ForEach(std::function<void(const K&, V&)> action) override {
    for (auto& [key, value] : data_) {
      action(key, value);
    }
  }

  std::optional<K> RandomKey(std::mt19937_64& rng) override {
    if (data_.empty()) return std::nullopt;
    // Start at a random bucket, walk forward to the first non-empty one, then
    // pick uniformly within it. Keys that follow runs of empty buckets are
    // favoured slightly, which is fine for sampling.
    std::uniform_int_distribution<std::size_t> pick_bucket(
        0, data_.bucket_count() - 1);
    std::size_t bucket = pick_bucket(rng);
    while (data_.bucket_size(bucket) == 0) {
      bucket = (bucket + 1) % data_.bucket_count();
    }
    std::uniform_int_distribution<std::size_t> pick_in_bucket(
        0, data_.bucket_size(bucket) - 1);
    return std::next(data_.begin(bucket),
                     static_cast<std::ptrdiff_t>(pick_in_bucket(rng)))
        ->first;
  }

 private:
  std::unordered_map<K, V> data_;
};

}  // namespace myredis

#endif  // MYREDIS_DATA_STRUCTURES_MAP_STANDARD_MAP_H_
