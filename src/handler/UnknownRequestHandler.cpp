#include "UnknownRequestHandler.h"

#include "HandlerHelpers.h"
#include "logger/Logger.h"
#include "respvalue/RespValues.h"

bool UnknownRequestHandler::IsHandler(const RespValue& resp_value) const {
  return true;
}

void UnknownRequestHandler::Handle(int client_fd, const RespValue& resp_value) {
  const std::string unknown_error =
      Error("Unknown subcommand or command").serialize();
  SendResponse(client_fd, unknown_error, logger_);
}

std::unique_ptr<Handler> UnknownRequestHandler::CreateHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& store,
    const std::shared_ptr<Logger>& logger) {
  return std::make_unique<UnknownRequestHandler>(store, logger);
}

UnknownRequestHandler::UnknownRequestHandler(
    const std::shared_ptr<Map<std::string, std::optional<std::string>>>& store,
    const std::shared_ptr<Logger>& logger)
    : logger_(logger) {}
