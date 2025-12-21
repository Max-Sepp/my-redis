#ifndef MY_REDIS_STRIPEDSETFACTORY_H
#define MY_REDIS_STRIPEDSETFACTORY_H

#include "MapBasedSet.h"
#include "SetFactory.h"
#include "StripedHashmap.h"

template <typename V>
class StripedSetFactory final : public SetFactory<V> {
  std::function<size_t(const V &)> hash_;

 public:
  explicit StripedSetFactory(std::function<size_t(const V &)> hash)
      : hash_(std::move(hash)) {}

  std::unique_ptr<Set<V>> CreateSet() override {
    std::unique_ptr<StripedHashmap<V, typename MapBasedSet<V>::Empty>>
        striped_map =
            std::make_unique<StripedHashmap<V, typename MapBasedSet<V>::Empty>>(
                DEFAULT_LOAD_FACTOR, hash_);

    return std::make_unique<MapBasedSet<V>>(std::move(striped_map));
  }
};

#endif  // MY_REDIS_STRIPEDSETFACTORY_H
