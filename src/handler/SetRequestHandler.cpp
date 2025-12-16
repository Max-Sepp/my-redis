#include "SetRequestHandler.h"

#include "HandlerHelpers.h"
#include "requests/SetRequest.h"

bool SetRequestHandler::IsHandler(const RespValue& resp_value) const {
  return SetRequest::IsRequest(resp_value);
}

void SetRequestHandler::Handle(const int client_fd,
                               const RespValue& resp_value) {
  const SetRequest request(resp_value);
  try {
    this->data_->Insert(request.getKey(), request.getValue());
  } catch (std::exception&) {
    SendResponse(client_fd, INTERNAL_ERROR_RESP, logger_);
    return;
  }
  SendResponse(client_fd, OK_RESP, logger_);
}

std::unique_ptr<Handler> SetRequestHandler::CreateHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
    const std::shared_ptr<Logger>& logger) {
  return std::make_unique<SetRequestHandler>(data, logger);
}

SetRequestHandler::SetRequestHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
    const std::shared_ptr<Logger>& logger)
    : data_(data), logger_(logger) {}
