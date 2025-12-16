#ifndef MY_REDIS_PUBSUBCHANNELS_H
#define MY_REDIS_PUBSUBCHANNELS_H

#include <atomic>
#include <string>
#include <unordered_set>

#include "logger/Logger.h"
#include "respvalue/RespValue.h"
#include "store/Map.h"

class PubSubChannels {
 public:
  PubSubChannels(std::unique_ptr<Map<std::string, std::unordered_set<int>>>
                     channel_to_client_fds,
                 std::unique_ptr<Map<int, std::unique_ptr<std::atomic_int>>>
                     client_fd_to_connection,
                 const std::shared_ptr<Logger>& logger);
  [[nodiscard]] int Subscribe(int client_fd, const std::string& channel) const;
  void Publish(const std::string& channel, const RespValue& resp_value) const;
  [[nodiscard]] int Unsubscribe(int client_fd,
                                const std::string& channel) const;

 private:
  // TODO: Concurrency problem around concurrent access to unordered_set
  const std::unique_ptr<Map<std::string, std::unordered_set<int>>>
      channel_to_client_fds_;
  const std::unique_ptr<Map<int, std::unique_ptr<std::atomic_int>>>
      client_fd_to_connection_;

  const std::shared_ptr<Logger> logger_;
};

#endif  // MY_REDIS_PUBSUBCHANNELS_H
