#include "UnsubscribeRequestHandler.h"

#include "HandlerHelpers.h"
#include "pubsub/PubSubResponse.h"
#include "requests/UnsubscribeRequest.h"

UnsubscribeRequestHandler::UnsubscribeRequestHandler(
    std::shared_ptr<PubSubChannels> pub_sub_channels,
    std::shared_ptr<Logger> logger)
    : pub_sub_channels_(std::move(pub_sub_channels)),
      logger_(std::move(logger)) {}

bool UnsubscribeRequestHandler::IsHandler(const RespValue& resp_value) const {
  return UnsubscribeRequest::IsRequest(resp_value);
}

void UnsubscribeRequestHandler::Handle(const int client_fd,
                                       const RespValue& resp_value) {
  const UnsubscribeRequest request(resp_value);
  for (int i = 0; i < request.numChannels(); i++) {
    const std::string& channel = request.getChannel(i);
    const int num_channels = pub_sub_channels_->Unsubscribe(client_fd, channel);
    SendResponse(
        client_fd,
        PubSubResponse("unsubscribe", channel, num_channels).serialize(),
        logger_);
  }
}
