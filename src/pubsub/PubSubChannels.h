#ifndef MY_REDIS_PUBSUBCHANNELS_H
#define MY_REDIS_PUBSUBCHANNELS_H

#include <atomic>
#include <memory>
#include <string>
#include <unordered_set>

#include "logger/Logger.h"
#include "respvalue/RespValue.h"
#include "store/Map.h"
#include "store/Set.h"
#include "store/SetFactory.h"

class PubSubChannels {
 public:
  PubSubChannels(std::unique_ptr<Map<std::string, std::unique_ptr<Set<int>>>>
                     channel_to_client_fds,
                 std::unique_ptr<SetFactory<int>> set_factory,
                 std::unique_ptr<Map<int, std::unique_ptr<std::atomic_int>>>
                     client_fd_to_connection,
                 const std::shared_ptr<Logger>& logger);
  [[nodiscard]] int Subscribe(int client_fd, const std::string& channel) const;
  void Publish(const std::string& channel, const RespValue& resp_value) const;
  [[nodiscard]] int Unsubscribe(int client_fd,
                                const std::string& channel) const;

 private:
  const std::unique_ptr<Map<std::string, std::unique_ptr<Set<int>>>>
      channel_to_client_fds_;
  const std::unique_ptr<SetFactory<int>> set_factory_;

  const std::unique_ptr<Map<int, std::unique_ptr<std::atomic_int>>>
      client_fd_to_connection_;

  const std::shared_ptr<Logger> logger_;
};

#endif  // MY_REDIS_PUBSUBCHANNELS_H
