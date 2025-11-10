#include "Handler.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

#include "requests/DelRequest.h"
#include "respvalue/RespValue.h"
#include "respvalue/RespValues.h"

const std::string INTERNAL_ERROR_RESP = "-ERR internal error\r\n";
const std::string OK_RESP = "+OK\r\n";

Handler::Handler(
    std::unique_ptr<Map<std::string, std::optional<std::string>>> data,
    const std::shared_ptr<Logger>& logger)
    : data_(std::move(data)), logger_(logger) {}

void Handler::Handle(const int client_fd, const sockaddr_in client_addr) const {
  close(client_fd);
}

void Handler::HandleGetRequest(const int client_fd,
                               const GetRequest& request) const {
  const std::unique_ptr<std::optional<std::string>> value =
      data_->LookUp(request.getKey());
  if (value == nullptr) {
    // Not Found
    const std::string not_found = NullBulkString().serialize();
    SendResponse(client_fd, not_found);
  } else {
    // Found
    const std::string result = BulkString(*value).serialize();
    SendResponse(client_fd, result);
  }
}

void Handler::HandleSetRequest(const int client_fd,
                               const SetRequest& request) const {
  try {
    this->data_->Insert(request.getKey(), request.getValue());
  } catch (std::exception& _) {
    SendResponse(client_fd, INTERNAL_ERROR_RESP);
    return;
  }
  SendResponse(client_fd, OK_RESP);
}
void Handler::HandleDelRequest(const int client_fd,
                               const DelRequest& request) const {
  try {
    this->data_->Remove(request.getKey());
  } catch (std::exception& _) {
    SendResponse(client_fd, INTERNAL_ERROR_RESP);
    return;
  }
  SendResponse(client_fd, Integer(1).serialize());
}

void Handler::UnknownCommand(const int client_fd) const {
  const std::string unknown_error =
      Error("Unknown subcommand or command").serialize();
  SendResponse(client_fd, unknown_error);
}

void Handler::SendResponse(const int client_fd,
                           const std::string& message) const {
  logger_->Log("Sending: " + message);
  send(client_fd, message.c_str(), message.size(), 0);
}
