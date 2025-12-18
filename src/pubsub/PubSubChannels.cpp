#include "PubSubChannels.h"

#include "HandlerHelpers.h"

PubSubChannels::PubSubChannels(
    std::unique_ptr<Map<std::string, std::unique_ptr<Set<int>>>>
        channel_to_client_fds,
    std::unique_ptr<SetFactory<int>> set_factory,
    std::unique_ptr<Map<int, std::unique_ptr<std::atomic_int>>>
        client_fd_to_connection,
    const std::shared_ptr<Logger>& logger)
    : channel_to_client_fds_(std::move(channel_to_client_fds)),
      set_factory_(std::move(set_factory)),
      client_fd_to_connection_(std::move(client_fd_to_connection)),
      logger_(logger) {}

int PubSubChannels::Subscribe(const int client_fd,
                              const std::string& channel) const {
  const auto channels_client_fds = channel_to_client_fds_->LookUp(channel);
  if (channels_client_fds == std::nullopt) {
    std::unique_ptr<Set<int>> set = set_factory_->CreateSet();
    set->Insert(client_fd);
    channel_to_client_fds_->Insert(channel, std::move(set));
  } else {
    channels_client_fds->get()->Insert(client_fd);
  }
  const auto current_connections = client_fd_to_connection_->LookUp(client_fd);

  if (current_connections == std::nullopt) {
    client_fd_to_connection_->Insert(client_fd,
                                     std::make_unique<std::atomic_int>(1));
    return 1;
  }
  return ++(*current_connections->get());
}
void PubSubChannels::Publish(const std::string& channel,
                             const RespValue& resp_value) const {
  const auto channels_client_fds = channel_to_client_fds_->LookUp(channel);

  if (channels_client_fds == std::nullopt) return;

  const std::string serialized_resp_value = resp_value.serialize();
  for (const int client_fd : *channels_client_fds->get()) {
    SendResponse(client_fd, serialized_resp_value, logger_);
  }
}

int PubSubChannels::Unsubscribe(const int client_fd,
                                const std::string& channel) const {
  const auto current_connections = client_fd_to_connection_->LookUp(client_fd);
  if (current_connections == std::nullopt) return 0;

  const auto channels_client_fds = channel_to_client_fds_->LookUp(channel);

  if (channels_client_fds == std::nullopt)
    return current_connections->get()->load();

  channels_client_fds->get()->Remove(client_fd);
  return --(*current_connections->get());
}