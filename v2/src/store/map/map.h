#ifndef MYREDIS_STORE_MAP_H_
#define MYREDIS_STORE_MAP_H_

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace myredis {

constexpr double kDefaultLoadFactor = 0.75;
constexpr int kDefaultCapacity = 16;

template <typename K, typename V>
class Map {
 public:
  using const_iterator =
      typename std::vector<std::pair<const K&, const V&>>::const_iterator;

  virtual ~Map() = default;

  virtual std::optional<std::reference_wrapper<const V>> LookUp(
      const K& key) = 0;

  virtual void Insert(K key, V value) = 0;

  virtual void Remove(const K& key) = 0;

  virtual void ForEach(std::function<void(const K&, const V&)> action) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_STORE_MAP_H_
