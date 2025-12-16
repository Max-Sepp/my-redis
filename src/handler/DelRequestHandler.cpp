#include "DelRequestHandler.h"

#include "HandlerHelpers.h"
#include "requests/DelRequest.h"
#include "respvalue/RespValues.h"

bool DelRequestHandler::IsHandler(const RespValue& resp_value) const {
  return DelRequest::IsRequest(resp_value);
}

void DelRequestHandler::Handle(const int client_fd,
                               const RespValue& resp_value) {
  const DelRequest request(resp_value);
  try {
    this->data_->Remove(request.getKey());
  } catch (std::exception&) {
    SendResponse(client_fd, INTERNAL_ERROR_RESP, logger_);
    return;
  }
  SendResponse(client_fd, Integer(1).serialize(), logger_);
}

std::unique_ptr<Handler> DelRequestHandler::CreateHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
    const std::shared_ptr<Logger>& logger) {
  return std::make_unique<DelRequestHandler>(data, logger);
}

DelRequestHandler::DelRequestHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
    const std::shared_ptr<Logger>& logger)
    : data_(data), logger_(logger) {}
