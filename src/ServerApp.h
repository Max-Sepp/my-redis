#ifndef SERVER_APP_H
#define SERVER_APP_H

#include <memory>

#include "RequestExecutor.h"

class ServerApp {
 public:
  ServerApp();
  ~ServerApp();

  // Non-copyable
  ServerApp(const ServerApp&) = delete;
  ServerApp& operator=(const ServerApp&) = delete;

  // Starts the server loop; returns process exit code.
  [[nodiscard]] int start() const;

 private:
  void handle_new_connection() const;
  void handle_request(int client_fd) const;

  std::shared_ptr<Logger> logger_;
  std::unique_ptr<RequestExecutor> request_executor_;

  int epoll_fd_ = -1;
  int server_fd_ = -1;
};

#endif  // SERVER_APP_H
