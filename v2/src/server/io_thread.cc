#include "io_thread.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <stdexcept>
#include <utility>
#include <variant>

namespace myredis {

namespace {
constexpr std::size_t kMaxEvents = 64;
constexpr std::size_t kReadBufSize = 4096;

// recv/send on a non-blocking, level-triggered socket can report these to mean
// "nothing more right now" rather than a real failure. EINTR is grouped here
// because the socket stays readable/writable and epoll will fire again.
bool WouldBlockOrInterrupted(const ssize_t result) {
  return result < 0 &&
         (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
}
}  // namespace

IoThread::IoThread(const EventFd& command_event)
    : epoll_fd_(epoll_create1(EPOLL_CLOEXEC)), command_event_(command_event) {
  // Register the inbox wakeup so the main thread can hand us work while we are
  // blocked in epoll_wait.
  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = inbox_event_.Fd();
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, inbox_event_.Fd(), &ev);
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
  // AssignConnection is trivially copyable, so reconstructing it on every
  // attempt is safe even though a failed Push consumes its argument.
  while (!inbox_.Push(AssignConnection{client_fd})) {
    std::this_thread::yield();
  }
  inbox_event_.Notify();
}

void IoThread::PostResponse(int client_fd, std::string bytes) {
  // Build the message once and copy on retry: a failed Push consumes its
  // argument, so retrying with a moved value would lose `bytes`.
  InboxMsg msg = WriteResponse{client_fd, std::move(bytes)};
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
      const epoll_event& ev = events[i];
      const bool is_inbox = ev.data.fd == inbox_event_.Fd();
      const bool is_error = (ev.events & (EPOLLERR | EPOLLHUP)) != 0;

      if (is_inbox) {
        inbox_event_.Drain();
        DrainInbox();
      } else if (is_error) {
        CloseConnection(ev.data.fd, /*notify_main=*/true);
      } else {
        if (ev.events & EPOLLIN) HandleReadable(ev.data.fd);
        // HandleReadable may have closed the connection; only write if it is
        // still alive.
        if ((ev.events & EPOLLOUT) && connections_.contains(ev.data.fd)) {
          HandleWritable(ev.data.fd);
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

  connections_.try_emplace(client_fd, client_fd);

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = client_fd;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
}

void IoThread::HandleWriteResponse(const WriteResponse& response) {
  const auto it = connections_.find(response.fd);
  if (it == connections_.end()) return;  // client already disconnected
  Connection& conn = it->second;
  conn.out_buffer.append(response.bytes);
  FlushOutBuffer(conn);
}

void IoThread::HandleReadable(int client_fd) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;
  Connection& conn = it->second;

  const bool alive = ReadIntoParseQueue(conn);

  // Parsing (and any std::invalid_argument for malformed RESP framing) happens
  // in PopValue, inside EmitParsedCommands.
  bool well_formed = true;
  try {
    EmitParsedCommands(conn);
  } catch (const std::invalid_argument&) {
    well_formed = false;  // drop the connection on a protocol error
  }

  if (!alive || !well_formed) CloseConnection(client_fd, /*notify_main=*/true);
}

bool IoThread::ReadIntoParseQueue(Connection& conn) {
  std::array<char, kReadBufSize> buf{};
  ssize_t n = recv(conn.fd, buf.data(), buf.size(), MSG_DONTWAIT);
  while (n > 0) {
    conn.parse_queue.PushString(
        std::string(buf.data(), static_cast<std::size_t>(n)));
    n = recv(conn.fd, buf.data(), buf.size(), MSG_DONTWAIT);
  }

  if (n == 0) return false;  // peer performed an orderly shutdown
  // n < 0: drained (still alive) for would-block/interrupt, otherwise fatal.
  return WouldBlockOrInterrupted(n);
}

void IoThread::EmitParsedCommands(Connection& conn) {
  while (std::optional<RespValue> request = conn.parse_queue.PopValue()) {
    Emit(Command{conn.fd, std::move(*request)});
  }
}

void IoThread::HandleWritable(int client_fd) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;
  FlushOutBuffer(it->second);
}

void IoThread::FlushOutBuffer(Connection& conn) {
  // Write as much of out_buffer as the socket will currently accept, tracking
  // exactly how many bytes were consumed so the remainder can be retried on
  // EPOLLOUT. (SendAll is unsuitable here: it cannot report partial progress
  // when a non-blocking send would block.)
  std::size_t sent = 0;
  bool would_block = false;
  while (sent < conn.out_buffer.size() && !would_block) {
    const ssize_t n = send(conn.fd, conn.out_buffer.data() + sent,
                           conn.out_buffer.size() - sent,
                           MSG_NOSIGNAL | MSG_DONTWAIT);
    if (n > 0) {
      sent += static_cast<std::size_t>(n);
    } else if (WouldBlockOrInterrupted(n)) {
      would_block = true;
    } else {
      conn.out_buffer.clear();
      CloseConnection(conn.fd, /*notify_main=*/true);  // fatal write error
      return;
    }
  }

  conn.out_buffer.erase(0, sent);
  // Subscribe to EPOLLOUT only while bytes remain to be flushed.
  UpdateEpoll(conn.fd, /*writable=*/!conn.out_buffer.empty());
}

void IoThread::CloseConnection(int client_fd, bool notify_main) {
  const auto it = connections_.find(client_fd);
  if (it == connections_.end()) return;
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
  close(client_fd);
  connections_.erase(it);
  if (notify_main) Emit(Disconnect{client_fd});
}

void IoThread::Emit(const OutboxMsg& msg) {
  // outbox_ is fixed-capacity. A failed Push consumes its argument, so we copy
  // `msg` on each attempt to preserve it for retry. The common (uncontended)
  // path performs one copy and one Notify.
  // TODO(v2): proper backpressure (pause EPOLLIN on our connections while the
  // queue is full) and avoid the per-message copy.
  while (!outbox_.Push(msg)) {
    command_event_.Notify();  // nudge the main thread to drain
    std::this_thread::yield();
  }
  command_event_.Notify();
}

void IoThread::UpdateEpoll(int client_fd, bool writable) const {
  epoll_event ev{};
  ev.events = EPOLLIN | (writable ? EPOLLOUT : 0);
  ev.data.fd = client_fd;
  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
}

}  // namespace myredis
