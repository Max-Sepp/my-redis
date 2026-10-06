#ifndef MY_REDIS_SINGLE_CONSUMER_PRODUCER_QUEUE_H
#define MY_REDIS_SINGLE_CONSUMER_PRODUCER_QUEUE_H
#include <array>
#include <atomic>
#include <concepts>
#include <optional>
#include <utility>

#include "concurrent/cache_line.h"

namespace myredis {

template <typename T, std::size_t N>
class SingleConsumerProducerQueue {
  std::array<std::optional<T>, N + 1> buffer_{};
  alignas(kCacheLineSize) std::atomic<size_t> head_{0};
  alignas(kCacheLineSize) std::atomic<size_t> tail_{0};

  static size_t increment(const size_t index) { return (index + 1) % (N + 1); }

 public:
  // `element` is only moved from (or copied) when the push succeeds, so a
  // caller can retry a failed Push with the same object.
  template <typename U>
    requires std::constructible_from<T, U&&>
  [[nodiscard]] bool Push(U&& element) {
    const size_t tail = tail_.load(std::memory_order_relaxed);  // we own tail_
    if (increment(tail) == head_.load(std::memory_order_acquire)) {
      return false;
    }

    buffer_[tail].emplace(std::forward<U>(element));
    tail_.store(increment(tail), std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::optional<T> Pop() {
    const size_t head = head_.load(std::memory_order_relaxed);  // we own head_
    if (head == tail_.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    std::optional<T> element = std::move(buffer_[head]);
    buffer_[head].reset();
    head_.store(increment(head), std::memory_order_release);
    return element;
  }
};
}  // namespace myredis

#endif  // MY_REDIS_SINGLE_CONSUMER_PRODUCER_QUEUE_H
