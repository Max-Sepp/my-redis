#ifndef MYREDIS_SERVER_IO_THREAD_H_
#define MYREDIS_SERVER_IO_THREAD_H_

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "concurrent/backpressure_queue.h"
#include "concurrent/event_fd.h"
#include "concurrent/single_consumer_producer_queue.h"
#include "server/connection.h"
#include "server/messages.h"

namespace myredis {

// One IO thread of the server. It owns a set of client connections and does
// only socket IO and RESP parsing: it reads bytes, parses them into RESP
// requests that it hands to the main thread, and writes back the response
// bytes the main thread produces. Command execution happens on the main thread.
//
// Concurrency contract:
//   - inbox_  is SPSC: the main thread is the sole producer (via PostAssign /
//     PostResponse), this IO thread is the sole consumer.
//   - outbox_ is SPSC: this IO thread is the sole producer, the main thread is
//     the sole consumer (via GetOutboxMsg).
//   - command_event (owned by the server, shared by all IO threads) is
//   signalled
//     after pushing to outbox_ to wake the main thread.
//
// Backpressure: this thread never blocks on a full outbox_. While outbox_ has
// a backlog the thread stops reading from its sockets, so a slow main thread
// pushes back on clients through TCP instead of stalling this thread. When the
// main thread finds outbox_ empty in GetOutboxMsg, it signals
// outbox_.SpaceAvailableEvent() to wake this thread to hand over the rest and
// start reading again.
class IoThread {
 public:
  static constexpr std::size_t kQueueCapacity = 2;

  // `command_event` is the main thread's wakeup; it is signalled whenever this
  // thread enqueues an OutboxMsg. It must outlive this IoThread.
  explicit IoThread(const EventFd& command_event);
  ~IoThread();

  IoThread(const IoThread&) = delete;
  IoThread& operator=(const IoThread&) = delete;

  // Spawn the worker thread running the epoll loop.
  void Start();
  // Signal the worker to stop and join it.
  void Stop();

  // --- Called from the main thread only -------------------------------------

  // Hand a freshly accepted client fd to this thread.
  void PostAssign(int client_fd);
  // Hand response bytes destined for `client_fd` (owned by this thread).
  void PostResponse(int client_fd, std::string bytes);

  // Pop the next IO -> main message, or std::nullopt if none are pending. The
  // main thread (the sole consumer) calls this in a loop when command_event
  // fires. Finding outbox_ empty also wakes this thread if it has messages
  // waiting for room in outbox_.
  std::optional<OutboxMsg> GetOutboxMsg() { return outbox_.Pop(); }

 private:
  void Run();

  // Inbox handling (main -> this thread).
  void DrainInbox();
  void HandleAssign(int client_fd);
  void HandleWriteResponse(const WriteResponse& response);

  // Client socket handling.
  void HandleReadable(int client_fd);
  void HandleWritable(int client_fd);
  // After a Send/Flush on conn: closes the connection on a fatal write error,
  // otherwise (de)registers EPOLLOUT depending on whether bytes remain.
  void FinishWrite(int client_fd, const Connection& conn, bool write_ok);

  void CloseConnection(int client_fd, bool notify_main);
  // Push to outbox_ and pause reading while it has a backlog. Never blocks.
  void Emit(OutboxMsg msg);
  // Stop or resume reading from every client socket by dropping or restoring
  // EPOLLIN.
  void SetReadingPaused(bool paused);

  // The epoll mask for a client fd: EPOLLIN unless reading is paused, plus
  // EPOLLOUT iff `writable`.
  std::uint32_t InterestMask(bool writable) const;
  // Apply InterestMask(writable) to an already registered client fd.
  void UpdateEpoll(int client_fd, bool writable) const;

  int epoll_fd_ = -1;
  EventFd inbox_event_;  // main -> this thread wakeup

  SingleConsumerProducerQueue<InboxMsg, kQueueCapacity> inbox_;
  BackpressureQueue<OutboxMsg, kQueueCapacity> outbox_;
  // True while client sockets are registered without EPOLLIN.
  bool reading_paused_ = false;

  std::unordered_map<int, Connection> connections_;
  std::thread thread_;
  std::atomic<bool> running_{false};
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_IO_THREAD_H_
