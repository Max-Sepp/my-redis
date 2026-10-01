#ifndef MYREDIS_CONCURRENT_BACKPRESSURE_QUEUE_H_
#define MYREDIS_CONCURRENT_BACKPRESSURE_QUEUE_H_

#include <atomic>
#include <cstddef>
#include <deque>
#include <optional>
#include <utility>

#include "concurrent/event_fd.h"
#include "concurrent/single_consumer_producer_queue.h"

namespace myredis {

// A single-producer single-consumer queue whose producer never blocks.
// Elements that do not fit in the fixed-capacity queue wait in a backlog, and
// the producer is expected to stop creating new work while HasBacklog() is
// true (the backlog itself is unbounded). When the consumer finds the queue
// empty in Pop, it signals SpaceAvailableEvent so the producer can Flush the
// rest.
//
// Concurrency contract:
//   - queue_ is SPSC: the producer pushes via Push / Flush, the consumer pops
//     via Pop.
//   - backlog_ is touched only by the producer.
//   - consumer_event (owned by the caller) is signalled after pushing to
//     queue_ to wake the consumer.
//   - space_available_event_ is signalled by Pop to wake the producer once
//     queue_ has room for its backlog.
template <typename T, std::size_t N>
class BackpressureQueue {
 public:
  // `consumer_event` is the consumer's wakeup; it is signalled whenever
  // elements are pushed to the queue. It must outlive this BackpressureQueue.
  explicit BackpressureQueue(const EventFd& consumer_event)
      : consumer_event_(consumer_event) {}

  BackpressureQueue(const BackpressureQueue&) = delete;
  BackpressureQueue& operator=(const BackpressureQueue&) = delete;

  // --- Called from the producer only ----------------------------------------

  // Queue `element` behind the backlog and hand over as much as fits. Never
  // blocks.
  void Push(T element) {
    // Always queue behind backlog_, even when queue_ has room, so elements
    // reach the consumer in the order they were pushed.
    backlog_.push_back(std::move(element));
    Flush();
  }

  // Hand over as much of the backlog as fits.
  void Flush() {
    bool pushed = MoveBacklogToQueue();
    if (!backlog_.empty()) {
      // queue_ is full. Ask the consumer to wake us once it has emptied it,
      // then try once more: it may have emptied queue_ just after our failed
      // push, before it could see the request, and would then never wake us.
      //
      // Memory ordering: each side stores to one variable and then loads the
      // other.
      //   - Producer (here): store space_wanted_ = true, then load queue_'s
      //     head_ in the retry below (the second MoveBacklogToQueue, inside
      //     queue_.Push). The first MoveBacklogToQueue is not involved; its
      //     failure is only what brings us here.
      //   - Consumer (Pop): store queue_'s head_ in the Pops that freed slots,
      //     then load space_wanted_ in the exchange.
      // The wakeup is lost only if both loads miss the other side's store: the
      // retry still sees queue_ full and the consumer still reads false. So on
      // each side the load must not be reordered before the store above it.
      space_wanted_.store(true, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_seq_cst);
      pushed = MoveBacklogToQueue() || pushed;
    }
    if (pushed) consumer_event_.Notify();
  }

  // Whether any elements are still waiting for room in queue_.
  [[nodiscard]] bool HasBacklog() const { return !backlog_.empty(); }

  // Signalled when queue_ has room for the backlog. Register it in the
  // producer's epoll set and Flush when it fires.
  [[nodiscard]] const EventFd& SpaceAvailableEvent() const {
    return space_available_event_;
  }

  // --- Called from the consumer only ----------------------------------------

  // Pop the next element, or std::nullopt if none are pending. Finding the
  // queue empty also wakes the producer if it has a backlog.
  std::optional<T> Pop() {
    std::optional<T> element = queue_.Pop();
    if (!element) {
      // Keeps the head_ stores from the Pops that freed slots ahead of the
      // space_wanted_ load in the exchange. Pairs with the fence in Flush,
      // which explains why it is needed.
      std::atomic_thread_fence(std::memory_order_seq_cst);
      if (space_wanted_.exchange(false, std::memory_order_relaxed)) {
        space_available_event_.Notify();
      }
    }
    return element;
  }

 private:
  // Push the front of backlog_ into queue_ until one of them runs out. Returns
  // whether anything was pushed.
  bool MoveBacklogToQueue() {
    bool pushed = false;
    while (!backlog_.empty() && queue_.Push(std::move(backlog_.front()))) {
      backlog_.pop_front();
      pushed = true;
    }
    return pushed;
  }

  SingleConsumerProducerQueue<T, N> queue_;
  // Elements that did not fit in queue_, oldest first.
  std::deque<T> backlog_;
  // Set by the producer when queue_ is full, cleared by the consumer in Pop,
  // which then signals space_available_event_.
  std::atomic<bool> space_wanted_{false};

  const EventFd& consumer_event_;  // producer -> consumer wakeup
  EventFd space_available_event_;  // consumer -> producer wakeup
};

}  // namespace myredis

#endif  // MYREDIS_CONCURRENT_BACKPRESSURE_QUEUE_H_
