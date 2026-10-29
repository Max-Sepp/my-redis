#ifndef MYREDIS_DATA_STRUCTURES_MAP_MAP_H_
#define MYREDIS_DATA_STRUCTURES_MAP_MAP_H_

#include <functional>
#include <optional>
#include <random>

namespace myredis {

constexpr double kDefaultLoadFactor = 0.75;
constexpr int kDefaultCapacity = 16;

template <typename K, typename V>
class Map {
 public:
  virtual ~Map() = default;

  virtual std::optional<std::reference_wrapper<V>> LookUp(const K& key) = 0;

  virtual void Insert(K key, V value) = 0;

  virtual void Remove(const K& key) = 0;

  virtual void ForEach(std::function<void(const K&, V&)> action) = 0;

  // Returns a copy of a randomly chosen key, or std::nullopt if the map is
  // empty. Intended for sampling (e.g. active key expiry), so implementations
  // may trade exact uniformity for speed.
  virtual std::optional<K> RandomKey(std::mt19937_64& rng) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_DATA_STRUCTURES_MAP_MAP_H_
