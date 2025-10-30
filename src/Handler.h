#ifndef MY_REDIS_HANDLER_H
#define MY_REDIS_HANDLER_H

#include <netinet/in.h>

#include <optional>

#include "logging/Logger.h"
#include "requests/GetRequest.h"
#include "requests/SetRequest.h"
#include "store/Map.h"

class Handler {
 public:
  explicit Handler(
      std::unique_ptr<Map<std::string, std::optional<std::string>>> data,
      const std::shared_ptr<Logger>& logger);
  void Handle(int client_fd, sockaddr_in client_addr) const;

 private:
  std::unique_ptr<Map<std::string, std::optional<std::string>>> data_;
  std::shared_ptr<Logger> logger_;

  void HandleGetRequest(int client_fd, const GetRequest& request) const;
  void HandleSetRequest(int client_fd, const SetRequest& request) const;
  void UnknownCommand(int client_fd) const;
  void SendResponse(int client_fd, const std::string& message) const;
};

#endif  // MY_REDIS_HANDLER_H
