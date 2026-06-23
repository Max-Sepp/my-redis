#include "server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include "store/standard_map.h"

namespace myredis {

namespace {
constexpr std::size_t kMaxEvents = 64;
constexpr int kConnectionBacklog = 16;

// Creates a non-blocking TCP socket bound to `port` and listening on all
// interfaces. Returns the fd, or -1 on failure (with a message on std::cerr).
int CreateListenSocket(int port) {
  const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    std::perror("socket");
    return -1;
  }

  constexpr int enable = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

  const int flags = fcntl(listen_fd, F_GETFL, 0);
  fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::perror("bind");
    close(listen_fd);
    return -1;
  }

  if (listen(listen_fd, kConnectionBacklog) != 0) {
    std::perror("listen");
    close(listen_fd);
    return -1;
  }

  return listen_fd;
}

unsigned NumIoThreads() {
  // Reserve one core for the main (command-executing) thread.
  const unsigned hardware = std::thread::hardware_concurrency();
  return hardware > 1 ? hardware - 1 : 1;
}
}  // namespace

Server::Server(int port)
    : dispatcher_(std::make_unique<
                  StandardMap<std::string, std::optional<std::string>>>()),
      epoll_fd_(epoll_create1(EPOLL_CLOEXEC)),
      listen_fd_(CreateListenSocket(port)) {
  const unsigned num_io_threads = NumIoThreads();
  io_threads_.reserve(num_io_threads);
  for (unsigned i = 0; i < num_io_threads; ++i) {
    io_threads_.push_back(std::make_unique<IoThread>(command_event_));
  }

  if (epoll_fd_ < 0 || listen_fd_ < 0) return;

  // Watch the listen socket (new connections) and the command eventfd
  // (IO threads have parsed requests to execute).
  for (const int watched_fd : {listen_fd_, command_event_.Fd()}) {
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = watched_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, watched_fd, &event);
  }
}

Server::~Server() {
  for (const auto& io_thread : io_threads_) io_thread->Stop();
  if (listen_fd_ >= 0) close(listen_fd_);
  if (epoll_fd_ >= 0) close(epoll_fd_);
}

int Server::Run() {
  if (epoll_fd_ < 0 || listen_fd_ < 0) {
    std::cerr << "Server failed to initialise\n";
    return EXIT_FAILURE;
  }

  for (const auto& io_thread : io_threads_) io_thread->Start();
  std::cout << "Server listening with " << io_threads_.size()
            << " IO thread(s)\n";

  std::array<epoll_event, kMaxEvents> events{};
  while (true) {
    const int nfds = epoll_wait(epoll_fd_, events.data(), events.size(), -1);
    if (nfds < 0) {
      if (errno == EINTR) continue;  // interrupted; just re-arm
      std::perror("epoll_wait");
      return EXIT_FAILURE;
    }

    for (int i = 0; i < nfds; ++i) {
      const epoll_event& event = events[i];
      if (event.data.fd == listen_fd_) {
        AcceptConnections();
      } else if (event.data.fd == command_event_.Fd()) {
        command_event_.Drain();
        ProcessCommands();
      }
    }
  }
}

void Server::AcceptConnections() {
  // Drain the accept backlog (the listen socket is non-blocking).
  int client_fd = accept(listen_fd_, nullptr, nullptr);
  while (client_fd >= 0) {
    AssignToIoThread(client_fd);
    client_fd = accept(listen_fd_, nullptr, nullptr);
  }
}

void Server::AssignToIoThread(int client_fd) {
  const std::size_t thread_index = next_thread_;
  next_thread_ = (next_thread_ + 1) % io_threads_.size();

  fd_to_thread_[client_fd] = static_cast<int>(thread_index);
  io_threads_[thread_index]->PostAssign(client_fd);
}

void Server::ProcessCommands() {
  // Drain every IO thread's outbox. (A single shared command eventfd wakes us;
  // we do not know which thread signalled, so we check them all.)
  for (const auto& io_thread : io_threads_) {
    while (std::optional<OutboxMsg> msg = io_thread->GetOutboxMsg()) {
      if (const auto* command = std::get_if<Command>(&*msg)) {
        ExecuteAndRespond(*command);
      } else if (const auto* disconnect = std::get_if<Disconnect>(&*msg)) {
        fd_to_thread_.erase(disconnect->fd);
      }
    }
  }
}

void Server::ExecuteAndRespond(const Command& command) {
  std::string response = Execute(command.value);

  const auto iter = fd_to_thread_.find(command.fd);
  if (iter == fd_to_thread_.end()) return;  // client disconnected meanwhile
  io_threads_[iter->second]->PostResponse(command.fd, std::move(response));
}

std::string Server::Execute(const RespValue& request) {
  return dispatcher_.Dispatch(request).Serialize();
}

}  // namespace myredis
