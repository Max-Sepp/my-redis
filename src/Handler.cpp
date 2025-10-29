#include "Handler.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

#include "respvalue/RespValue.h"
#include "respvalue/RespValues.h"

const std::string INTERNAL_ERROR_RESP = "-ERR internal error\r\n";
const std::string OK_RESP = "+OK\r\n";

Handler::Handler(
    std::unique_ptr<Map<std::string, std::optional<std::string>>> data,
    const std::shared_ptr<Logger>& logger)
    : data_(std::move(data)), logger_(logger) {}

void Handler::Handle(const int client_fd, const sockaddr_in client_addr) const {
  RecvBufferIterator begin(client_fd);
  const RecvBufferIterator end;

  while (begin != end) {
    std::unique_ptr<RespValue> resp;
    try {
      resp = std::make_unique<RespValue>(begin, end);
    } catch (std::invalid_argument& e) {
      logger_->Log(e.what());
    }

    const std::string addr = inet_ntoa(client_addr.sin_addr);
    logger_->Log("Connection from address: " + addr +
                 " Request was: " + resp->serialize());

    if (GetRequest::IsRequest(*resp))
      HandleGetRequest(client_fd, GetRequest(*resp));
    else if (SetRequest::IsRequest(*resp))
      HandleSetRequest(client_fd, SetRequest(*resp));
    else
      UnknownCommand(client_fd);
    break;
  }

  close(client_fd);
}

void Handler::HandleGetRequest(const int client_fd,
                               const GetRequest& request) const {
  const std::unique_ptr<std::optional<std::string>> value =
      data_->LookUp(request.getKey());
  if (value == nullptr) {
    // Not Found
    const std::string not_found = NullBulkString().serialize();
    send(client_fd, not_found.c_str(), not_found.size(), 0);
  } else {
    // Found
    const std::string result = BulkString(*value).serialize();
    send(client_fd, result.c_str(), result.size(), 0);
  }
}

void Handler::HandleSetRequest(int client_fd, const SetRequest& request) const {
  try {
    this->data_->Insert(request.getKey(), request.getValue());
  } catch (std::exception& _) {
    send(client_fd, INTERNAL_ERROR_RESP.c_str(), INTERNAL_ERROR_RESP.size(), 0);
    return;
  }
  logger_->Log("Sending: " + OK_RESP);
  send(client_fd, OK_RESP.c_str(), OK_RESP.size(), 0);
}

void Handler::UnknownCommand(const int client_fd) {
  const std::string unknown_error =
      Error("Unknown subcommand or command").serialize();
  send(client_fd, unknown_error.c_str(), unknown_error.size(), 0);
}
