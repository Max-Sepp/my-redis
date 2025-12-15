#include "GetRequestHandler.h"

#include "HandlerHelpers.h"
#include "requests/GetRequest.h"
#include "respvalue/RespValues.h"

bool GetRequestHandler::IsHandler(const RespValue& resp_value) const {
  return GetRequest::IsRequest(resp_value);
}

void GetRequestHandler::Handle(const int client_fd,
                               const RespValue& resp_value) {
  const GetRequest request(resp_value);
  const std::unique_ptr<std::optional<std::string>> value =
      data_->LookUp(request.getKey());
  if (value == nullptr) {
    // Not Found
    const std::string not_found = NullBulkString().serialize();
    SendResponse(client_fd, not_found, logger_);
  } else {
    // Found
    const std::string result = BulkString(*value).serialize();
    SendResponse(client_fd, result, logger_);
  }
}

std::unique_ptr<Handler> GetRequestHandler::CreateHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
    const std::shared_ptr<Logger>& logger) {
  return std::make_unique<GetRequestHandler>(data, logger);
}

GetRequestHandler::GetRequestHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
    const std::shared_ptr<Logger>& logger)
    : data_(data), logger_(logger) {}
