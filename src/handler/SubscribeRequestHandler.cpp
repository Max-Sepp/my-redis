#include "SubscribeRequestHandler.h"

#include "HandlerHelpers.h"
#include "pubsub/PubSubResponse.h"
#include "requests/SubscribeRequest.h"

SubscribeRequestHandler::SubscribeRequestHandler(
    std::shared_ptr<PubSubChannels> pub_sub_channels,
    std::shared_ptr<Logger> logger)
    : pub_sub_channels_(std::move(pub_sub_channels)),
      logger_(std::move(logger)) {}

bool SubscribeRequestHandler::IsHandler(const RespValue& resp_value) const {
  return SubscribeRequest::IsRequest(resp_value);
}

void SubscribeRequestHandler::Handle(const int client_fd,
                                     const RespValue& resp_value) {
  const SubscribeRequest request(resp_value);
  for (int i = 0; i < request.numChannels(); i++) {
    const std::string& channel = request.getChannel(i);
    const int num_channels = pub_sub_channels_->Subscribe(client_fd, channel);
    SendResponse(client_fd,
                 PubSubResponse("subscribe", channel, num_channels).serialize(),
                 logger_);
  }
}
