#ifndef MYREDIS_STORE_STANDARD_MAP_H_
#define MYREDIS_STORE_STANDARD_MAP_H_

#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

#include "store/map.h"

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

  std::optional<std::reference_wrapper<const V>> LookUp(const K& key) override {
    const auto iter = data_.find(key);
    if (iter == data_.end()) return std::nullopt;
    return std::optional<std::reference_wrapper<const V>>(
        std::cref(iter->second));
  }

  void Insert(K key, V value) override {
    data_[std::move(key)] = std::move(value);
  }

  void Remove(const K& key) override { data_.erase(key); }

  void ForEach(std::function<void(const K&, const V&)> action) override {
    for (const auto& [key, value] : data_) {
      action(key, value);
    }
  }

 private:
  std::unordered_map<K, V> data_;
};

}  // namespace myredis

#endif  // MYREDIS_STORE_STANDARD_MAP_H_
