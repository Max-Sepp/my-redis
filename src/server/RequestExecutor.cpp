#include "RequestExecutor.h"

#include <sys/socket.h>

#include "respvalue/RespValues.h"

RequestExecutor::RequestExecutor(std::unique_ptr<HandlerDispatcher> dispatcher)
    : RequestExecutor(std::move(dispatcher),
                      std::thread::hardware_concurrency() == 0
                          ? 1U
                          : std::thread::hardware_concurrency()) {}

RequestExecutor::RequestExecutor(std::unique_ptr<HandlerDispatcher> dispatcher,
                                 const unsigned int num_threads)
    : dispatcher_(std::move(dispatcher)) {
  assert(num_threads > 0);
  for (unsigned int i = 0; i < num_threads; i++) {
    workers_.emplace_back(Worker, std::ref(jobs_), std::cref(dispatcher_),
                          std::cref(client_fd_to_connection_));
  }
}

void RequestExecutor::Submit(const int client_fd, const std::string& request) {
  jobs_.Push(std::pair(client_fd, request));
}

void RequestExecutor::Remove(int client_fd) {
  // TODO handle removal currently just leak data
}

void RequestExecutor::Worker(
    ConcurrentQueue<std::pair<int, std::string>>& jobs,
    const std::unique_ptr<HandlerDispatcher>& dispatcher,
    const std::unique_ptr<Map<int, std::unique_ptr<ClientConnection>>>&
        client_fd_to_connection) {
  while (true) {
    const auto [client_fd, request] = jobs.Pop();

    auto maybe_connection = client_fd_to_connection->LookUp(client_fd);
    if (maybe_connection == std::nullopt) {
      client_fd_to_connection->Insert(client_fd,
                                      std::make_unique<ClientConnection>());
      maybe_connection = client_fd_to_connection->LookUp(client_fd);
    }

    assert(maybe_connection != std::nullopt);
    HandleConnection(dispatcher, client_fd, maybe_connection->get(), request);
  }
}

void RequestExecutor::HandleConnection(
    const std::unique_ptr<HandlerDispatcher>& dispatcher, const int client_fd,
    const std::unique_ptr<ClientConnection>& connection,
    const std::string& request) {
  // Add the request to the resp value queue.
  // Check if a thread is processing requests if so move to next connection.
  // Otherwise, handle the connection.
  {
    std::scoped_lock lock(connection->mutex_);

    connection->resp_value_queue_.PushString(request);
    if (connection->being_processed) return;

    connection->being_processed = true;
  }

  // Repeated pop requests of the queue if any available.
  while (true) {
    assert(connection->being_processed);
    RespValue resp_request = NullBulkString();
    {
      std::scoped_lock lock(connection->mutex_);

      const std::optional<RespValue> maybe_resp_request =
          connection->resp_value_queue_.PopValue();

      if (maybe_resp_request == std::nullopt) {
        connection->being_processed = false;
        return;
      }
      resp_request = maybe_resp_request.value();
    }

    dispatcher->DispatchRequest(client_fd, resp_request);
  }
}
