#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <thread>

#include "Handler.h"
#include "logging/FileLogger.h"
#include "store/CoarseGrainConcurrentMapWrapper.h"
#include "store/Hash.h"
#include "store/LinearProbingHashmap.h"

int main() {
  std::shared_ptr<Logger> logger = std::make_shared<FileLogger>(std::cout);
  auto underlying_map = std::make_unique<
      LinearProbingHashmap<std::string, std::optional<std::string>>>(
      0.75, string_hash);
  auto concurrent_map = std::make_unique<
      CoarseGrainConcurrentMapWrapper<std::string, std::optional<std::string>>>(
      std::move(underlying_map));
  const auto handler =
      std::make_unique<Handler>(std::move(concurrent_map), logger);

  const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&server_addr),
           sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  sockaddr_in client_addr{};
  int client_addr_len = sizeof(client_addr);
  while (true) {
    logger->Log("Waiting for a client to connect...");
    int client_fd =
        accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr),
               reinterpret_cast<socklen_t *>(&client_addr_len));
    logger->Log("Client connected");

    std::thread clientThread(&Handler::Handle, handler.get(), client_fd,
                             client_addr);
    clientThread.detach();
  }

  close(server_fd);

  return 0;
}