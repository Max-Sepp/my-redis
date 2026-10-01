#include "io_thread.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <utility>
#include <variant>
#include <vector>

namespace myredis {

namespace {
constexpr std::size_t kMaxEvents = 64;
}  // namespace

IoThread::IoThread(const EventFd& command_event)
    : epoll_fd_(epoll_create1(EPOLL_CLOEXEC)),
      outbox_(command_event) {
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
  while (!inbox_.Push(AssignConnection{client_fd})) {
    std::this_thread::yield();
  }
  inbox_event_.Notify();
}

void IoThread::PostResponse(int client_fd, std::string bytes) {
  // Push copies `msg` into the queue only once it succeeds, so retrying with
  // the same object is safe.
  InboxMsg msg = WriteResponse{.fd = client_fd, .bytes = std::move(bytes)};
  while (!inbox_.Push(msg)) {
    std::this_thread::yield();
  }
  inbox_event_.Notify();
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
  while (std::optional<InboxMsg> msg = inbox_.Pop()) {
    if (const auto* assign = std::get_if<AssignConnection>(&*msg)) {
      HandleAssign(assign->fd);
    } else if (const auto* response = std::get_if<WriteResponse>(&*msg)) {
      HandleWriteResponse(*response);
    }
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

void IoThread::HandleWriteResponse(const WriteResponse& response) {
  const auto it = connections_.find(response.fd);
  if (it == connections_.end()) return;  // client already disconnected
  Connection& conn = it->second;
  FinishWrite(response.fd, conn, conn.Send(response.bytes));
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
