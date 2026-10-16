#include "io_thread.h"

#include <fcntl.h>
#include <netinet/in.h>  // IWYU pragma: keep (IPPROTO_TCP)
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "concurrent/event_fd.h"
#include "server/connection.h"
#include "server/messages.h"

namespace myredis {

namespace {
constexpr std::size_t kMaxEvents = 64;

// Records are packed back to back, so headers may be unaligned; hence memcpy.
void WriteHeader(const std::span<std::byte> slot, const InboxHeader& header) {
  std::memcpy(slot.data(), &header, sizeof(header));
}

InboxHeader ReadHeader(const std::byte* record) {
  InboxHeader header{};
  std::memcpy(&header, record, sizeof(header));
  return header;
}

std::string_view PayloadOf(const std::byte* record, const InboxHeader& header) {
  return {reinterpret_cast<const char*>(record + sizeof(header)),
          header.length};
}
}  // namespace

IoThread::IoThread(const EventFd& command_event)
    : epoll_fd_(epoll_create1(EPOLL_CLOEXEC)), outbox_(command_event) {
  // Register the inbox and outbox wakeups so the main thread can hand us work,
  // or tell us outbox_ has room, while we are blocked in epoll_wait.
  for (const int wakeup_fd :
       {inbox_event_.Fd(), outbox_.SpaceAvailableEvent().Fd()}) {
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = wakeup_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd, &event);
  }
}

IoThread::~IoThread() {
  Stop();
  // Best-effort cleanup of any still-open client sockets.
  for (const auto& [client_fd, conn] : connections_) close(client_fd);
  if (epoll_fd_ >= 0) close(epoll_fd_);
}

void IoThread::Start() {
  running_.store(true, std::memory_order_relaxed);
  thread_ = std::thread(&IoThread::Run, this);
}

void IoThread::Stop() {
  running_.store(false, std::memory_order_relaxed);
  inbox_event_.Notify();  // wake epoll_wait so it observes running_ == false
  if (thread_.joinable()) thread_.join();
}

void IoThread::PostAssign(int client_fd) {
  assert(!writer_open_);
  const std::span<std::byte> slot = WaitForInboxSlot(0);
  WriteHeader(slot, {.type = InboxRecordType::kAssignConnection,
                     .fd = client_fd,
                     .length = 0});
  inbox_.Commit(sizeof(InboxHeader));
  inbox_event_.Notify();
}

IoThread::ResponseWriter IoThread::BeginResponse(int client_fd) {
  return {*this, client_fd};
}

void IoThread::PostResponse(int client_fd, std::string_view bytes) {
  BeginResponse(client_fd).Append(bytes);
}

IoThread::ResponseWriter::ResponseWriter(IoThread& owner, const int client_fd)
    : owner_(owner), client_fd_(client_fd) {
  assert(!owner_.writer_open_);
  owner_.writer_open_ = true;
}

void IoThread::ResponseWriter::Append(std::string_view bytes) {
  while (!bytes.empty()) {
    // Reserve all the free space so later appends fill the same record.
    if (slot_.empty()) slot_ = owner_.WaitForInboxSlot(kInboxBytes);
    const std::size_t room = slot_.size() - sizeof(InboxHeader) - used_;
    const std::size_t count = std::min(room, bytes.size());
    std::memcpy(slot_.data() + sizeof(InboxHeader) + used_, bytes.data(),
                count);
    used_ += count;
    bytes.remove_prefix(count);
    if (used_ + sizeof(InboxHeader) == slot_.size()) CommitRecord();
  }
}

void IoThread::ResponseWriter::Finish() {
  CommitRecord();
  owner_.writer_open_ = false;
}

void IoThread::ResponseWriter::CommitRecord() {
  // A reservation is only opened to write bytes, so an open one is never empty.
  if (slot_.empty()) return;
  // The payload is already in place; only its length was unknown until now.
  WriteHeader(slot_, {.type = InboxRecordType::kWriteResponse,
                      .fd = client_fd_,
                      .length = static_cast<std::uint32_t>(used_)});
  owner_.inbox_.Commit(sizeof(InboxHeader) + used_);
  owner_.inbox_event_.Notify();
  slot_ = {};
  used_ = 0;
}

std::span<std::byte> IoThread::WaitForInboxSlot(
    const std::size_t payload_size) {
  // A record must make progress: at least one payload byte, if it has any.
  const std::size_t smallest_useful_slot =
      sizeof(InboxHeader) + std::min<std::size_t>(payload_size, 1);
  while (true) {
    const std::span<std::byte> slot =
        inbox_.Reserve(sizeof(InboxHeader) + payload_size);
    if (slot.size() >= smallest_useful_slot) return slot;
    inbox_.Commit(0);
    std::this_thread::yield();
  }
}

void IoThread::Run() {
  std::array<epoll_event, kMaxEvents> events{};
  while (running_.load(std::memory_order_relaxed)) {
    const int nfds = epoll_wait(epoll_fd_, events.data(), events.size(), -1);
    if (nfds < 0) {
      if (errno == EINTR) continue;  // interrupted; just re-arm
      break;                         // unrecoverable epoll error
    }

    for (int i = 0; i < nfds; ++i) {
      const epoll_event& event = events[i];
      const bool is_inbox = event.data.fd == inbox_event_.Fd();
      const bool is_outbox_space_available =
          event.data.fd == outbox_.SpaceAvailableEvent().Fd();
      const bool is_error = (event.events & (EPOLLERR | EPOLLHUP)) != 0;

      if (is_inbox) {
        inbox_event_.Drain();
        DrainInbox();
      } else if (is_outbox_space_available) {
        outbox_.SpaceAvailableEvent().Drain();
        outbox_.Flush();
        SetReadingPaused(outbox_.HasBacklog());
      } else if (is_error) {
        CloseConnection(event.data.fd, /*notify_main=*/true);
      } else {
        // An EPOLLIN reported before reading was paused may still be in this
        // batch. Skip it; the socket stays readable, so epoll reports it again
        // once reading resumes.
        if (static_cast<bool>(event.events & EPOLLIN) && !reading_paused_)
          HandleReadable(event.data.fd);
        // HandleReadable may have closed the connection; only write if it is
        // still alive.
        if (static_cast<bool>(event.events & EPOLLOUT) &&
            connections_.contains(event.data.fd)) {
          HandleWritable(event.data.fd);
        }
      }
    }
  }
}

void IoThread::DrainInbox() {
  // Records committed after this snapshot are announced by a later Notify.
  const std::size_t pending_bytes = inbox_.Size();
  if (pending_bytes == 0) return;
  const auto batch = inbox_.Pop(pending_bytes);
  assert(batch);

  const std::byte* record = batch->begin();
  while (record != batch->end()) {
    const InboxHeader header = ReadHeader(record);
    HandleRecord(header, PayloadOf(record, header));
    record += sizeof(header) + header.length;
  }
}

void IoThread::HandleRecord(const InboxHeader& header,
                            const std::string_view payload) {
  switch (header.type) {
    case InboxRecordType::kAssignConnection:
      HandleAssign(header.fd);
      break;
    case InboxRecordType::kWriteResponse:
      HandleWriteResponse(header.fd, payload);
      break;
  }
}

void IoThread::HandleAssign(int client_fd) {
  // Client sockets must be non-blocking for the epoll loop.
  const int flags = fcntl(client_fd, F_GETFL, 0);
  fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

  // Without this, a request/response pair split across two small writes (the
  // common case here: a client sends a request, we send a small reply) hits
  // the classic Nagle/delayed-ACK interaction and can stall a connection for
  // multiples of the ~40ms delayed-ACK timer. Real Redis sets this on every
  // client socket for the same reason.
  constexpr int enable = 1;
  setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

  connections_.try_emplace(client_fd, client_fd);

  epoll_event event{};
  event.events = InterestMask(/*writable=*/false);
  event.data.fd = client_fd;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event);
}

void IoThread::HandleWriteResponse(const int client_fd,
                                   const std::string_view bytes) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;  // client already disconnected
  Connection& conn = it->second;
  FinishWrite(client_fd, conn, conn.Send(bytes));
}

void IoThread::HandleReadable(int client_fd) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;

  Connection::ReadResult result = it->second.Read();

  // Coalesce every request drained from this read into a single outbox message
  // so a pipelined batch costs one push + Notify, not one per command.
  if (!result.requests.empty())
    Emit(CommandBatch{.fd = client_fd, .values = std::move(result.requests)});

  if (!result.keep_open) CloseConnection(client_fd, /*notify_main=*/true);
}

void IoThread::HandleWritable(int client_fd) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;
  Connection& conn = it->second;
  FinishWrite(client_fd, conn, conn.Flush());
}

void IoThread::FinishWrite(int client_fd, const Connection& conn,
                           const bool write_ok) {
  if (!write_ok) {
    CloseConnection(client_fd, /*notify_main=*/true);  // fatal write error
    return;
  }
  // Subscribe to EPOLLOUT only while bytes remain to be flushed.
  UpdateEpoll(client_fd, /*writable=*/conn.HasPendingWrites());
}

void IoThread::CloseConnection(int client_fd, bool notify_main) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
  close(client_fd);
  connections_.erase(it);
  if (notify_main) Emit(Disconnect{client_fd});
}

void IoThread::Emit(OutboxMsg msg) {
  outbox_.Push(std::move(msg));
  SetReadingPaused(outbox_.HasBacklog());
}

void IoThread::SetReadingPaused(const bool paused) {
  if (paused == reading_paused_) return;
  reading_paused_ = paused;
  // EPOLLERR / EPOLLHUP are always reported, so a paused client that
  // disconnects is still noticed.
  for (const auto& [client_fd, conn] : connections_) {
    UpdateEpoll(client_fd, /*writable=*/conn.HasPendingWrites());
  }
}

std::uint32_t IoThread::InterestMask(const bool writable) const {
  const std::uint32_t readable_bits = reading_paused_ ? 0 : EPOLLIN;
  const std::uint32_t writable_bits = writable ? EPOLLOUT : 0;
  return readable_bits | writable_bits;
}

void IoThread::UpdateEpoll(int client_fd, bool writable) const {
  epoll_event event{};
  event.events = InterestMask(writable);
  event.data.fd = client_fd;
  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &event);
}

}  // namespace myredis
