#include "RequestExecutor.h"

RequestExecutor::RequestExecutor(std::unique_ptr<HandlerDispatcher> dispatcher,
                                 const unsigned int num_threads)
    : dispatcher_(std::move(dispatcher)) {
  assert(num_threads > 0);
  for (unsigned int i = 0; i < num_threads; i++) {
    workers_.emplace_back(Worker, std::ref(client_fds_to_handle_),
                          std::cref(dispatcher_),
                          std::cref(client_fd_to_connection_));
  }
}

RequestExecutor::RequestExecutor(std::unique_ptr<HandlerDispatcher> dispatcher)
    : RequestExecutor(std::move(dispatcher),
                      std::thread::hardware_concurrency() == 0
                          ? 1U
                          : std::thread::hardware_concurrency()) {}

void RequestExecutor::Submit(const int client_fd, const std::string& request) {
  // Check for a client_fd of this type
  if (const auto connection_result =
          client_fd_to_connection_->LookUp(client_fd);
      !connection_result.has_value()) {
    // New connection so add to client fds to connections.
    auto connection = std::make_unique<ClientConnection>();
    AddRequestToRespQueue(connection, request);
    client_fd_to_connection_->Insert(client_fd, std::move(connection));
  } else {
    // Connection already exists
    AddRequestToRespQueue(connection_result->get(), request);
  }

  // Signal to threads potential processing of a client fd.
  client_fds_to_handle_.Push(client_fd);
}

void RequestExecutor::Remove(const int client_fd) {
  // TODO currently just leak data.
}

void RequestExecutor::AddRequestToRespQueue(
    const std::unique_ptr<ClientConnection>& connection,
    const std::string& request) {
  std::scoped_lock lock(connection->queue_mutex_);
  connection->resp_value_queue_.PushString(request);
}

void RequestExecutor::Worker(
    ConcurrentQueue<int>& client_fds_to_handle,
    const std::unique_ptr<HandlerDispatcher>& dispatcher,
    const std::unique_ptr<Map<int, std::unique_ptr<ClientConnection>>>&
        client_fd_to_connection) {
  while (true) {
    const int client_fd = client_fds_to_handle.Pop();

    if (const auto connection_result =
            client_fd_to_connection->LookUp(client_fd);
        connection_result.has_value()) {
      HandleConnection(client_fd, connection_result->get(), dispatcher);
    }
  }
}

void RequestExecutor::HandleConnection(
    const int client_fd, const std::unique_ptr<ClientConnection>& connection,
    const std::unique_ptr<HandlerDispatcher>& dispatcher) {
  std::scoped_lock lock(connection->queue_mutex_);

  for (std::optional<RespValue> maybe_resp_value =
           connection->resp_value_queue_.PopValue();
       maybe_resp_value != std::nullopt;
       maybe_resp_value = connection->resp_value_queue_.PopValue()) {
    const RespValue& resp_value = maybe_resp_value.value();
    dispatcher->DispatchRequest(client_fd, resp_value);
  }
}
