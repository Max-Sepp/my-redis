#ifndef MY_REDIS_MAP_H
#define MY_REDIS_MAP_H

#include <memory>

template <typename K, typename V>
class Map {
 public:
  virtual ~Map() = default;

  virtual std::unique_ptr<V> lookUp(const K &key) = 0;

  virtual void insert(const K &key, const V &value) = 0;

  virtual void remove(const K &key) = 0;
};

#endif  // MY_REDIS_MAP_H
