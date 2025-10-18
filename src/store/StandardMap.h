#ifndef MY_REDIS_STANDARDMAP_H
#define MY_REDIS_STANDARDMAP_H
#include <unordered_map>

#include "Map.h"

template <typename K, typename V>
class StandardMap final : public Map<K, V> {
 public:
  StandardMap(const StandardMap& other)
      : Map<K, V>(other), data(other.underlyingMap) {}
  StandardMap(StandardMap&& other) noexcept
      : Map<K, V>(std::move(other)), data(std::move(other.underlyingMap)) {}
  StandardMap& operator=(const StandardMap& other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    data = other.underlyingMap;
    return *this;
  }
  StandardMap& operator=(StandardMap&& other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    data = std::move(other.underlyingMap);
    return *this;
  }

  StandardMap() = default;

  std::unique_ptr<V> lookUp(const K& key) override {
    const auto it = data.find(key);
    if (it == data.end()) return nullptr;
    return std::make_unique<V>(it->second);
  }

  void insert(const K& key, const V& value) override { data[key] = value; }

  void remove(const K& key) override { data.erase(key); }

 private:
  std::unordered_map<K, V> data;
};

#endif  // MY_REDIS_STANDARDMAP_H
