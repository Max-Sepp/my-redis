#include "PublishRequestHandler.h"

#include "HandlerHelpers.h"
#include "requests/PublishRequest.h"
#include "respvalue/RespValues.h"

PublishRequestHandler::PublishRequestHandler(
    std::shared_ptr<PubSubChannels> pub_sub_channels,
    std::shared_ptr<Logger> logger)
    : pub_sub_channels_(std::move(pub_sub_channels)),
      logger_(std::move(logger)) {}

bool PublishRequestHandler::IsHandler(const RespValue& resp_value) const {
  return PublishRequest::IsRequest(resp_value);
}

void PublishRequestHandler::Handle(const int client_fd,
                                   const RespValue& resp_value) {
  const PublishRequest request(resp_value);
  const int num_published_to =
      pub_sub_channels_->Publish(request.getChannel(), request.getMessage());
  SendResponse(client_fd, Integer(num_published_to).serialize(), logger_);
}
