#ifndef MY_REDIS_SETFACTORY_H
#define MY_REDIS_SETFACTORY_H
#include <memory>

#include "Set.h"

template <typename V>
class SetFactory {
 public:
  virtual ~SetFactory() = default;
  virtual std::unique_ptr<Set<V>> CreateSet() = 0;
};

#endif  // MY_REDIS_SETFACTORY_H
