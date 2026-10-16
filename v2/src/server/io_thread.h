#ifndef MYREDIS_SERVER_IO_THREAD_H_
#define MYREDIS_SERVER_IO_THREAD_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "concurrent/backpressure_queue.h"
#include "concurrent/batched_ring_buffer.h"
#include "concurrent/event_fd.h"
#include "resp_value/byte_sink.h"
#include "resp_value/resp_value.h"
#include "server/connection.h"
#include "server/messages.h"

namespace myredis {

// One IO thread of the server. It owns a set of client connections and does
// only socket IO and RESP parsing: it reads bytes, parses them into RESP
// requests that it hands to the main thread, and writes back the response
// bytes the main thread produces. Command execution happens on the main thread.
//
// Concurrency contract:
//   - inbox_  is an SPSC byte ring of InboxHeader-framed records: the main
//     thread is the sole producer (via PostAssign / BeginResponse), this IO
//     thread is the sole consumer and pops everything pending as one batch.
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
  // Responses larger than this still get through, split across records.
  static constexpr std::size_t kInboxBytes = std::size_t{1} << 16;

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

  // Streams response bytes for one client straight into inbox_, splitting
  // them across records when the free space runs out. The IO thread sees the
  // bytes once a record fills or the writer is finished or destroyed.
  class ResponseWriter : public ByteSink {
   public:
    ResponseWriter(const ResponseWriter&) = delete;
    ResponseWriter& operator=(const ResponseWriter&) = delete;
    ~ResponseWriter() override {
      // Finish only throws on inbox misuse, a bug we cannot recover from here.
      try {
        Finish();
      } catch (...) {
        std::terminate();
      }
    }

    void Append(std::string_view bytes) override;
    // Publishes any bytes not yet handed to the IO thread.
    void Finish();

   private:
    friend class IoThread;
    ResponseWriter(IoThread& owner, int client_fd);
    // Publishes the open reservation as one record, if there is one.
    void CommitRecord();

    IoThread& owner_;
    int client_fd_;
    std::span<std::byte> slot_;  // the open reservation, empty if none
    std::size_t used_ = 0;       // payload bytes written into slot_
  };

  // Hand a freshly accepted client fd to this thread.
  void PostAssign(int client_fd);
  // Open a writer for response bytes destined for `client_fd` (owned by this
  // thread). Only one writer may be open at a time, and PostAssign must not be
  // called while it is.
  ResponseWriter BeginResponse(int client_fd);
  // Hand response bytes destined for `client_fd`. The bytes are copied into
  // inbox_, so they need not outlive the call.
  void PostResponse(int client_fd, std::string_view bytes);

  // Pop the next IO -> main message, or std::nullopt if none are pending. The
  // main thread (the sole consumer) calls this in a loop when command_event
  // fires. Finding outbox_ empty also wakes this thread if it has messages
  // waiting for room in outbox_.
  std::optional<OutboxMsg> GetOutboxMsg() { return outbox_.Pop(); }

 private:
  void Run();

  // Main thread only.
  std::span<std::byte> WaitForInboxSlot(std::size_t payload_size);

  // Inbox handling (main -> this thread).
  void DrainInbox();
  // `payload` points into inbox_ and is only valid during the call.
  void HandleRecord(const InboxHeader& header, std::string_view payload);
  void HandleAssign(int client_fd);
  void HandleWriteResponse(int client_fd, std::string_view bytes);

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

  BatchRingBuffer<std::byte, kInboxBytes> inbox_;
  BackpressureQueue<OutboxMsg, kQueueCapacity> outbox_;
  std::thread thread_;
  std::unordered_map<int, Connection> connections_;
  int epoll_fd_ = -1;
  EventFd inbox_event_;       // main -> this thread wakeup
  bool writer_open_ = false;  // main thread only
  // True while client sockets are registered without EPOLLIN.
  bool reading_paused_ = false;
  std::atomic<bool> running_{false};
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_IO_THREAD_H_
