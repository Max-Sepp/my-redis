#ifndef MY_REDIS_MAPBASEDSET_H
#define MY_REDIS_MAPBASEDSET_H

#include <memory>

#include "Map.h"
#include "Set.h"

template <typename V>
class MapBasedSet final : public Set<V> {
 public:
  struct Empty {};

  explicit MapBasedSet(std::unique_ptr<Map<V, Empty>>& map)
      : map_(std::move(map)) {}

  bool Contains(const V& element) override {
    return map_->LookUp(element) != std::nullopt;
  }

  void Insert(V element) override { map_->Insert(element, Empty()); }

  void Remove(const V& element) override { map_->Remove(element); }

  std::unique_ptr<Map<V, Empty>> map_;
};

#endif  // MY_REDIS_MAPBASEDSET_H
