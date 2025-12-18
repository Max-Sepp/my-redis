#ifndef MY_REDIS_STRIPEDSETFACTORY_H
#define MY_REDIS_STRIPEDSETFACTORY_H
#include "MapBasedSet.h"
#include "SetFactory.h"
#include "StripedHashmap.h"

template <typename V>
class StripedSetFactory final : public SetFactory<V> {
 public:
  std::unique_ptr<Set<V>> CreateSet() override {
    return std::make_unique<MapBasedSet<V>>(std::make_unique<StripedHashmap>(
        DEFAULT_LOAD_FACTOR,
        [](const V& element) { return std::hash(element); }));
  };
};

#endif  // MY_REDIS_STRIPEDSETFACTORY_H
