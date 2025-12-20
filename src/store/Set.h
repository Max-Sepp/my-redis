#ifndef MY_REDIS_SET_H
#define MY_REDIS_SET_H

template <typename V>
class Set {
 public:
  virtual ~Set() = default;

  virtual bool Contains(const V& element) = 0;

  virtual void Insert(V element) = 0;

  virtual void Remove(const V& element) = 0;

  virtual void ForEach(std::function<void(const V&)> action) = 0;
};

#endif  // MY_REDIS_SET_H
