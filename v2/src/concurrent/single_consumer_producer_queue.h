#ifndef MY_REDIS_SINGLE_CONSUMER_PRODUCER_QUEUE_H
#define MY_REDIS_SINGLE_CONSUMER_PRODUCER_QUEUE_H
#include <array>
#include <atomic>
#include <optional>

namespace myredis {

template <typename T, std::size_t N>
class SingleConsumerProducerQueue {
  std::array<std::optional<T>, N + 1> buffer_{};
  std::atomic<size_t> head_{0};
  std::atomic<size_t> tail_{0};

  static size_t increment(const size_t n) {
    return (n + 1) % (N+1);
  }

public:
  [[nodiscard]] bool Push(T element) {
    const size_t t = tail_.load(std::memory_order_relaxed);  // we own tail_
    if (increment(t) == head_.load(std::memory_order_acquire)) {
      return false;
    }

    buffer_[t] = std::move(element);
    tail_.store(increment(t), std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::optional<T> Pop() {
    const size_t h = head_.load(std::memory_order_relaxed); // we own head_
    if (h == tail_.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    std::optional<T> element = std::move(buffer_[h]);
    buffer_[h].reset();
    head_.store(increment(h), std::memory_order_release);
    return element;
  }
};
} // namespace myredis

#endif  // MY_REDIS_SINGLE_CONSUMER_PRODUCER_QUEUE_H
