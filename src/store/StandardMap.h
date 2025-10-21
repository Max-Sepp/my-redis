#ifndef MY_REDIS_STANDARDMAP_H
#define MY_REDIS_STANDARDMAP_H
#include <unordered_map>

#include "Map.h"

template <typename K, typename V>
class StandardMap final : public Map<K, V> {
 public:
  StandardMap(const StandardMap& other)
      : Map<K, V>(other), data_(other.underlyingMap) {}
  StandardMap(StandardMap&& other) noexcept
      : Map<K, V>(std::move(other)), data_(std::move(other.underlyingMap)) {}
  StandardMap& operator=(const StandardMap& other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    data_ = other.underlyingMap;
    return *this;
  }
  StandardMap& operator=(StandardMap&& other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    data_ = std::move(other.underlyingMap);
    return *this;
  }

  StandardMap() = default;

  std::unique_ptr<V> LookUp(const K& key) override {
    const auto it = data_.find(key);
    if (it == data_.end()) return nullptr;
    return std::make_unique<V>(it->second);
  }

  void Insert(const K& key, const V& value) override { data_[key] = value; }

  void Remove(const K& key) override { data_.erase(key); }

 private:
  std::unordered_map<K, V> data_;
};

#endif  // MY_REDIS_STANDARDMAP_H
