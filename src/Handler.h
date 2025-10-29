#ifndef MY_REDIS_HANDLER_H
#define MY_REDIS_HANDLER_H

#include <netinet/in.h>

#include <optional>

#include "requests/GetRequest.h"
#include "requests/SetRequest.h"
#include "store/Map.h"

class Handler {
 public:
  explicit Handler(
      std::unique_ptr<Map<std::string, std::optional<std::string>>> data);
  void Handle(int client_fd, sockaddr_in client_addr) const;

 private:
  std::unique_ptr<Map<std::string, std::optional<std::string>>> data_;

  void HandleGetRequest(int client_fd, const GetRequest& request) const;
  void HandleSetRequest(int client_fd, const SetRequest& request) const;
};

#endif  // MY_REDIS_HANDLER_H
