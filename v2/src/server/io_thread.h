#ifndef MYREDIS_SERVER_IO_THREAD_H_
#define MYREDIS_SERVER_IO_THREAD_H_

#include <atomic>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

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
//   - command_event (owned by the server, shared by all IO threads) is signalled
//     after pushing to outbox_ to wake the main thread.
class IoThread {
 public:
  static constexpr std::size_t kQueueCapacity = 1024;

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
  // fires.
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
  // Reads until the socket would block. Returns false if the connection should
  // be closed (peer shutdown or fatal error), true if it is still alive.
  bool ReadIntoParseQueue(Connection& conn);
  // Hands every fully-parsed request in the queue to the main thread.
  void EmitParsedCommands(Connection& conn);
  // Writes as much of conn.out_buffer as the socket accepts, then (de)registers
  // EPOLLOUT for whatever remains.
  void FlushOutBuffer(Connection& conn);

  void CloseConnection(int client_fd, bool notify_main);
  // Push to outbox_ and wake the main thread (spin-retries if outbox_ is full).
  void Emit(const OutboxMsg& msg);
  // Set epoll interest for a client fd: EPOLLIN, plus EPOLLOUT iff `writable`.
  void UpdateEpoll(int client_fd, bool writable) const;

  int epoll_fd_ = -1;
  EventFd inbox_event_;           // main -> this thread wakeup
  const EventFd& command_event_;  // this thread -> main wakeup (server-owned)

  SingleConsumerProducerQueue<InboxMsg, kQueueCapacity> inbox_;
  SingleConsumerProducerQueue<OutboxMsg, kQueueCapacity> outbox_;

  std::unordered_map<int, Connection> connections_;
  std::thread thread_;
  std::atomic<bool> running_{false};
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_IO_THREAD_H_
