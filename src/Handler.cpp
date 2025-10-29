#include "Handler.h"

#include <unistd.h>

#include <iostream>

#include "respvalue/RespValue.h"
#include "respvalue/RespValues.h"

Handler::Handler(
    std::unique_ptr<Map<std::string, std::optional<std::string>>> data)
    : data_(std::move(data)) {}

void Handler::Handle(const int client_fd, sockaddr_in client_addr) const {
  const RecvBufferIterator begin(client_fd);
  const RecvBufferIterator end;
  const RespValue resp(begin, end);

  if (GetRequest::IsRequest(resp))
    HandleGetRequest(client_fd, GetRequest(resp));
  if (SetRequest::IsRequest(resp))
    HandleSetRequest(client_fd, SetRequest(resp));

  close(client_fd);
}

void Handler::HandleGetRequest(const int client_fd,
                               const GetRequest& request) const {
  const std::unique_ptr<std::optional<std::string>> value =
      this->data_->LookUp(request.getKey());
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
  this->data_->Insert(request.getKey(), request.getValue());
}