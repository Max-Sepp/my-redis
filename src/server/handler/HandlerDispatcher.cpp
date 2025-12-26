#include "HandlerDispatcher.h"

#include <arpa/inet.h>
#include <unistd.h>

#include "respvalue/RespValue.h"

HandlerDispatcher::HandlerDispatcher(
    std::vector<std::unique_ptr<Handler>> handlers,
    const std::shared_ptr<Logger>& logger)
    : handlers_(std::move(handlers)), logger_(logger) {}

void HandlerDispatcher::DispatchRequest(const int client_fd,
                                        const RespValue& resp_value) const {
  logger_->Log("Received: " + resp_value.serialize());
  for (const auto& handler : handlers_) {
    if (handler->IsHandler(resp_value)) {
      handler->Handle(client_fd, resp_value);
      break;
    }
  }
}