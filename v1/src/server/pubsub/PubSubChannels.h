#ifndef MY_REDIS_PUBSUBCHANNELS_H
#define MY_REDIS_PUBSUBCHANNELS_H

#include <atomic>
#include <memory>
#include <string>

#include "logger/Logger.h"
#include "store/Map.h"
#include "store/Set.h"
#include "store/SetFactory.h"

class PubSubChannels {
 public:
  PubSubChannels(
      std::unique_ptr<Map<std::string, std::unique_ptr<Set<int>>>>
          channel_to_client_fds,
      std::unique_ptr<SetFactory<int>> set_factory,
      std::unique_ptr<Map<int, std::unique_ptr<int>>> client_fd_to_connection,
      const std::shared_ptr<Logger>& logger);
  // For all these methods mutual exclusion must be provided so that the
  // functions are not called simultaneously with the same client_fd.
  [[nodiscard]] int Subscribe(int client_fd, const std::string& channel) const;
  [[nodiscard]] int Publish(const std::string& channel,
                            const std::string& message) const;
  [[nodiscard]] int Unsubscribe(int client_fd,
                                const std::string& channel) const;

 private:
  const std::unique_ptr<Map<std::string, std::unique_ptr<Set<int>>>>
      channel_to_client_fds_;
  const std::unique_ptr<SetFactory<int>> set_factory_;

  const std::unique_ptr<Map<int, std::unique_ptr<int>>>
      client_fd_to_connection_;

  const std::shared_ptr<Logger> logger_;
};

#endif  // MY_REDIS_PUBSUBCHANNELS_H
