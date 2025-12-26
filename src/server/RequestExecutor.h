#ifndef MY_REDIS_REQUESTEXECUTOR_H
#define MY_REDIS_REQUESTEXECUTOR_H

#include <string>

#include "concurrent/ConcurrentQueue.h"
#include "handler/HandlerDispatcher.h"
#include "respvalue/RespValueQueue.h"
#include "store/Hash.h"
#include "store/Map.h"
#include "store/StripedHashmap.h"

class RequestExecutor {
 public:
  explicit RequestExecutor(std::unique_ptr<HandlerDispatcher> dispatcher);
  RequestExecutor(std::unique_ptr<HandlerDispatcher> dispatcher,
                  unsigned int num_threads);
  void Submit(int client_fd, const std::string& request);
  void Remove(int client_fd);

 private:
  std::vector<std::thread> workers_;

  std::unique_ptr<HandlerDispatcher> dispatcher_;

  ConcurrentQueue<std::pair<int, std::string>> jobs_ =
      ConcurrentQueue<std::pair<int, std::string>>();

  struct ClientConnection {
    // Mutex for mutual exclusion over checking being_processed and
    // resp_value_queue.
    std::mutex mutex_;
    bool being_processed{false};
    RespValueQueue resp_value_queue_;
  };
  std::unique_ptr<Map<int, std::unique_ptr<ClientConnection>>>
      client_fd_to_connection_ = std::make_unique<
          StripedHashmap<int, std::unique_ptr<ClientConnection>>>(
          DEFAULT_LOAD_FACTOR, int_hash);

  static void Worker(
      ConcurrentQueue<std::pair<int, std::string>>& jobs,
      const std::unique_ptr<HandlerDispatcher>& dispatcher,
      const std::unique_ptr<Map<int, std::unique_ptr<ClientConnection>>>&
          client_fd_to_connection);

  static void HandleConnection(
      const std::unique_ptr<HandlerDispatcher>& dispatcher, int client_fd,
      const std::unique_ptr<ClientConnection>& connection,
      const std::string& request);
};

#endif  // MY_REDIS_REQUESTEXECUTOR_H
