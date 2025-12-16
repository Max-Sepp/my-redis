#include "PubSubChannels.h"

#include "HandlerHelpers.h"

PubSubChannels::PubSubChannels(
    std::unique_ptr<Map<std::string, std::unordered_set<int>>>
        channel_to_client_fds,
    std::unique_ptr<Map<int, std::unique_ptr<std::atomic_int>>>
        client_fd_to_connection,
    const std::shared_ptr<Logger>& logger)
    : channel_to_client_fds_(std::move(channel_to_client_fds)),
      client_fd_to_connection_(std::move(client_fd_to_connection)),
      logger_(logger) {}

int PubSubChannels::Subscribe(const int client_fd,
                              const std::string& channel) const {
  const std::unique_ptr<std::unordered_set<int>> channels_client_fds =
      channel_to_client_fds_->LookUp(channel);
  if (channels_client_fds == nullptr) {
    channel_to_client_fds_->Insert(channel, std::unordered_set<int>(client_fd));
  } else {
    channels_client_fds->insert(client_fd);
  }
  const std::unique_ptr<std::unique_ptr<std::atomic_int>> current_connections =
      client_fd_to_connection_->LookUp(client_fd);

  if (current_connections == nullptr) {
    client_fd_to_connection_->Insert(client_fd,
                                     std::make_unique<std::atomic_int>(1));
    return 1;
  }
  return (*current_connections)->operator++();
}
void PubSubChannels::Publish(const std::string& channel,
                             const RespValue& resp_value) const {
  const std::unique_ptr<std::unordered_set<int>> channels_client_fds =
      channel_to_client_fds_->LookUp(channel);

  if (channels_client_fds == nullptr) return;

  const std::string serialized_resp_value = resp_value.serialize();
  for (const int client_fd : *channels_client_fds) {
    SendResponse(client_fd, serialized_resp_value, logger_);
  }
}

int PubSubChannels::Unsubscribe(const int client_fd,
                                const std::string& channel) const {
  const std::unique_ptr<std::unique_ptr<std::atomic_int>> current_connections =
      client_fd_to_connection_->LookUp(client_fd);
  if (current_connections == nullptr) return 0;

  const std::unique_ptr<std::unordered_set<int>> channels_client_fds =
      channel_to_client_fds_->LookUp(channel);

  if (channels_client_fds == nullptr) return (*current_connections)->load();

  channels_client_fds->erase(client_fd);
  return (*current_connections)->operator--();
}