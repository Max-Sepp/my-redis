#ifndef MY_REDIS_CONCURRENTQUEUE_H
#define MY_REDIS_CONCURRENTQUEUE_H

#include <condition_variable>
#include <queue>

template <typename T>
class ConcurrentQueue {
 public:
  void Push(const T& value) {
    std::unique_lock lock(mutex_);
    queue_.push(value);
    lock.unlock();
    is_empty_.notify_one();
  }

  T Pop() {
    std::unique_lock lock(mutex_);
    is_empty_.wait(lock, [this] { return !queue_.empty(); });
    T value = queue_.front();
    queue_.pop();
    return value;
  }

 private:
  std::condition_variable is_empty_;
  std::mutex mutex_;
  std::queue<T> queue_;
};

#endif  // MY_REDIS_CONCURRENTQUEUE_H
