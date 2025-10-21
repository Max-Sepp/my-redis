#ifndef MY_REDIS_MAP_H
#define MY_REDIS_MAP_H

#include <memory>

template <typename K, typename V>
class Map {
 public:
  virtual ~Map() = default;

  virtual std::unique_ptr<V> LookUp(const K &key) = 0;

  virtual void Insert(const K &key, const V &value) = 0;

  virtual void Remove(const K &key) = 0;
};

#endif  // MY_REDIS_MAP_H
