#ifndef MY_REDIS_PUBLISHREQUESTHANDLER_H
#define MY_REDIS_PUBLISHREQUESTHANDLER_H

#include <memory>

#include "Handler.h"
#include "logger/Logger.h"
#include "pubsub/PubSubChannels.h"

class PublishRequestHandler final : public Handler {
 public:
  PublishRequestHandler(std::shared_ptr<PubSubChannels> pub_sub_channels,
                        std::shared_ptr<Logger> logger);
  [[nodiscard]] bool IsHandler(const RespValue& resp_value) const override;
  void Handle(int client_fd, const RespValue& resp_value) override;

 private:
  const std::shared_ptr<PubSubChannels> pub_sub_channels_;
  const std::shared_ptr<Logger> logger_;
};

#endif  // MY_REDIS_PUBLISHREQUESTHANDLER_H
