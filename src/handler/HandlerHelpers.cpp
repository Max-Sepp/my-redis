#include "HandlerHelpers.h"

#include <sys/socket.h>

void SendResponse(const int client_fd, const std::string& message,
                  const std::shared_ptr<Logger>& logger) {
  logger->Log("Sending: " + message);
  send(client_fd, message.c_str(), message.size(), 0);
}
