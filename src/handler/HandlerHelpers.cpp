#include "HandlerHelpers.h"

#include <sys/socket.h>

void SendResponse(int client_fd, const std::string& message,
                  std::shared_ptr<Logger> logger) {
  logger->Log("Sending: " + message);
  send(client_fd, message.c_str(), message.size(), 0);
}
