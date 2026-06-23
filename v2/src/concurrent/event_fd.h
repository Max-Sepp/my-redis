#ifndef MYREDIS_CONCURRENT_EVENT_FD_H_
#define MYREDIS_CONCURRENT_EVENT_FD_H_

namespace myredis {

// A thin RAII wrapper around a Linux eventfd used as a cross-thread wakeup.
//
// The underlying fd is created non-blocking so it can be registered in an
// epoll set (level-triggered EPOLLIN) and drained without blocking. `Notify`
// is safe to call from any thread (eventfd maintains an atomic counter), so a
// single EventFd may have multiple producers signalling a single epoll-owning
// consumer that calls `Drain`.
class EventFd {
 public:
  EventFd();
  ~EventFd();

  // Non-copyable, non-movable: the fd lifetime is tied to this object and the
  // raw fd is handed to epoll, so we never want it duplicated or relocated.
  EventFd(const EventFd&) = delete;
  EventFd& operator=(const EventFd&) = delete;
  EventFd(EventFd&&) = delete;
  EventFd& operator=(EventFd&&) = delete;

  // The raw fd, suitable for epoll_ctl. -1 if creation failed.
  [[nodiscard]] int Fd() const { return fd_; }

  // Wake the consumer. Safe to call from any thread.
  void Notify() const;

  // Reset the counter to zero. Call from the consumer after epoll reports the
  // fd readable so it does not fire again until the next Notify.
  void Drain() const;

 private:
  int fd_ = -1;
};

}  // namespace myredis

#endif  // MYREDIS_CONCURRENT_EVENT_FD_H_
