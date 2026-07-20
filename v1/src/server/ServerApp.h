#ifndef SERVER_APP_H
#define SERVER_APP_H

#include <memory>

#include "RequestExecutor.h"

class ServerApp {
 public:
  // When enable_request_logging is false (the default) a no-op logger is used,
  // so the hot path does no per-request I/O. Pass true to log every request and
  // response to stdout via FileLogger.
  explicit ServerApp(bool enable_request_logging = false);
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
