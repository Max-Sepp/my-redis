#ifndef MY_REDIS_MAP_H
#define MY_REDIS_MAP_H

#include <functional>
#include <optional>

constexpr double DEFAULT_LOAD_FACTOR = 0.75;
constexpr int DEFAULT_CAPACITY = 16;

template <typename K, typename V>
class Map {
 public:
  virtual ~Map() = default;

  virtual std::optional<std::reference_wrapper<const V>> LookUp(
      const K &key) = 0;

  virtual void Insert(K key, V value) = 0;

  virtual void Remove(const K &key) = 0;
};

#endif  // MY_REDIS_MAP_H
