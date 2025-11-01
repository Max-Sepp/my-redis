#ifndef MY_REDIS_COARSEGRAINCONCURRENTMAPWRAPPER_H
#define MY_REDIS_COARSEGRAINCONCURRENTMAPWRAPPER_H
#include <memory>

#include "Map.h"

template <typename K, typename V>
class CoarseGrainConcurrentMapWrapper final : public Map<K, V> {
 public:
  explicit CoarseGrainConcurrentMapWrapper(std::unique_ptr<Map<K, V>> map)
      : underlying_map_(std::move(map)) {}

  std::unique_ptr<V> LookUp(const K &key) override {
    std::lock_guard lk(mutex_);
    return underlying_map_->LookUp(key);
  }

  void Insert(const K &key, const V &value) override {
    std::lock_guard lk(mutex_);
    underlying_map_->Insert(key, value);
  }

  void Remove(const K &key) override {
    std::lock_guard lk(mutex_);
    underlying_map_->Remove(key);
  }

 private:
  std::unique_ptr<Map<K, V>> underlying_map_;
  std::mutex mutex_;
};

#endif  // MY_REDIS_COARSEGRAINCONCURRENTMAPWRAPPER_H
