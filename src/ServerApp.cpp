#include "ServerApp.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#include "handler/DelRequestHandler.h"
#include "handler/GetRequestHandler.h"
#include "handler/PublishRequestHandler.h"
#include "handler/SetRequestHandler.h"
#include "handler/SubscribeRequestHandler.h"
#include "handler/UnknownRequestHandler.h"
#include "handler/UnsubscribeRequestHandler.h"
#include "logger/FileLogger.h"
#include "store/Hash.h"
#include "store/Map.h"
#include "store/StripedHashmap.h"
#include "store/StripedSetFactory.h"

constexpr int EXPECTED_NUMBER_OF_CONCURRENT_CONNECTIONS = 16;
constexpr int PORT = 6379;

ServerApp::ServerApp() {
  logger_ = std::make_shared<FileLogger>(std::cout);

  // Initialise request executor
  const auto store =
      std::make_shared<StripedHashmap<std::string, std::optional<std::string>>>(
          DEFAULT_LOAD_FACTOR, string_hash);
  const auto pub_sub_channels = std::make_shared<PubSubChannels>(
      std::make_unique<StripedHashmap<std::string, std::unique_ptr<Set<int>>>>(
          DEFAULT_LOAD_FACTOR, string_hash),
      std::make_unique<StripedSetFactory<int>>(int_hash),
      std::make_unique<StripedHashmap<int, std::unique_ptr<std::atomic_int>>>(
          DEFAULT_LOAD_FACTOR, int_hash),
      logger_);

  std::vector<std::unique_ptr<Handler>> handlers;
  handlers.push_back(std::make_unique<GetRequestHandler>(store, logger_));
  handlers.push_back(std::make_unique<SetRequestHandler>(store, logger_));
  handlers.push_back(std::make_unique<DelRequestHandler>(store, logger_));
  handlers.push_back(
      std::make_unique<SubscribeRequestHandler>(pub_sub_channels, logger_));
  handlers.push_back(
      std::make_unique<PublishRequestHandler>(pub_sub_channels, logger_));
  handlers.push_back(
      std::make_unique<UnsubscribeRequestHandler>(pub_sub_channels, logger_));
  handlers.push_back(std::make_unique<UnknownRequestHandler>(store, logger_));

  request_executor_ = std::make_unique<RequestExecutor>(
      std::make_unique<HandlerDispatcher>(std::move(handlers), logger_));

  // Initialise epoll instance
  epoll_fd_ = epoll_create(EXPECTED_NUMBER_OF_CONCURRENT_CONNECTIONS);
  if (epoll_fd_ < 0) {
    logger_->Error("Failed to create epoll instance");
    return;
  }

  // Create server
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    logger_->Error("Failed to create server socket");
    return;
  }

  // Set server to be non-blocking.
  const int flags = fcntl(server_fd_, F_GETFL, 0);
  fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

  // Bind the server to the port.
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);
  if (bind(server_fd_, reinterpret_cast<sockaddr *>(&server_addr),
           sizeof(server_addr)) != 0) {
    logger_->Error("Failed to bind to port 6379");
    return;
  }

  // List on the file descriptor.
  if (constexpr int connection_backlog = 5;
      listen(server_fd_, connection_backlog) != 0) {
    logger_->Error("listen failed");
    return;
  }

  // Add the server file descriptor to the epoll instance.
  epoll_event server_listen_event{};
  server_listen_event.events = EPOLLIN;
  server_listen_event.data.fd = server_fd_;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &server_listen_event);
}

ServerApp::~ServerApp() {
  if (server_fd_ >= 0) close(server_fd_);
  if (epoll_fd_ >= 0) close(epoll_fd_);
}

int ServerApp::start() const {
  if (epoll_fd_ < 0 || server_fd_ < 0) {
    return EXIT_FAILURE;
  }

  while (true) {
    constexpr size_t MAX_EVENTS = 16;
    std::array<epoll_event, MAX_EVENTS> events{};
    const int nfds = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, -1);
    if (nfds < 0) {
      // If epoll was interrupted then just call epoll again.
      if (errno == EINTR) continue;

      // Log actual error.
      logger_->Error("epoll_wait failed");
      return EXIT_FAILURE;
    }
    for (int i = 0; i < nfds; ++i) {
      if (events[i].data.fd == server_fd_)
        handle_new_connection();
      else
        handle_request(events[i].data.fd);
    }
  }

  return EXIT_SUCCESS;
}

void ServerApp::handle_new_connection() const {
  sockaddr_in client_addr{};
  int client_addr_len = sizeof(client_addr);

  const int client_fd =
      accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr),
             reinterpret_cast<socklen_t *>(&client_addr_len));
  logger_->Log("Client connected");

  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = client_fd;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event);
}

void ServerApp::handle_request(const int client_fd) const {
  constexpr size_t BUF_SIZE = 4096;
  std::array<char, BUF_SIZE> buf{};

  while (true) {
    const ssize_t number_bytes_read =
        recv(client_fd, buf.data(), BUF_SIZE, MSG_DONTWAIT);

    if (number_bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      // No more data right now.
      break;

    if (number_bytes_read > 0) {
      request_executor_->Submit(
          client_fd, std::string(buf.begin(), buf.begin() + number_bytes_read));
      // Continue reading until the socket would block.
    } else {
      // Peer closed connection or unrecoverable: remove from epoll, drop queue
      // and close fd.
      epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
      request_executor_->Remove(client_fd);
      close(client_fd);
      logger_->Log("Client closed connection");
      return;
    }
  }
}
