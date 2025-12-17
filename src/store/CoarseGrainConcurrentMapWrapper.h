#ifndef MY_REDIS_COARSEGRAINCONCURRENTMAPWRAPPER_H
#define MY_REDIS_COARSEGRAINCONCURRENTMAPWRAPPER_H

#include <memory>
#include <mutex>
#include <optional>

#include "Map.h"

template <typename K, typename V>
class CoarseGrainConcurrentMapWrapper final : public Map<K, V> {
 public:
  explicit CoarseGrainConcurrentMapWrapper(std::unique_ptr<Map<K, V>> map)
      : underlying_map_(std::move(map)) {}

  std::optional<std::reference_wrapper<const V>> LookUp(const K &key) override {
    std::lock_guard lock(mutex_);
    return underlying_map_->LookUp(key);
  }

  void Insert(K key, V value) override {
    std::lock_guard lock(mutex_);
    underlying_map_->Insert(std::move(key), std::move(value));
  }

  void Remove(const K &key) override {
    std::lock_guard lock(mutex_);
    underlying_map_->Remove(key);
  }

 private:
  std::unique_ptr<Map<K, V>> underlying_map_;
  std::mutex mutex_;
};

#endif  // MY_REDIS_COARSEGRAINCONCURRENTMAPWRAPPER_H
