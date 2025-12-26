#include "PubSubChannels.h"

#include "../HandlerHelpers.h"
#include "PubSubResponse.h"
#include "respvalue/RespValues.h"

PubSubChannels::PubSubChannels(
    std::unique_ptr<Map<std::string, std::unique_ptr<Set<int>>>>
        channel_to_client_fds,
    std::unique_ptr<SetFactory<int>> set_factory,
    std::unique_ptr<Map<int, std::unique_ptr<int>>> client_fd_to_connection,
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
    client_fd_to_connection_->Insert(client_fd, std::make_unique<int>(1));
    return 1;
  }

  return ++(*current_connections->get());
}

int PubSubChannels::Publish(
    const std::string& channel,  // NOLINT(*-easily-swappable-parameters)
    const std::string& message) const {
  const auto channels_client_fds = channel_to_client_fds_->LookUp(channel);

  if (channels_client_fds == std::nullopt) return 0;

  int number_clients_published_to = 0;
  const RespValue resp_message = BulkString(message);
  channels_client_fds->get()->ForEach([&number_clients_published_to,
                                       resp_message, this,
                                       channel](const int& client_fd) {
    number_clients_published_to++;
    SendResponse(client_fd,
                 PubSubResponse("message", channel, resp_message).serialize(),
                 logger_);
  });
  return number_clients_published_to;
}

int PubSubChannels::Unsubscribe(const int client_fd,
                                const std::string& channel) const {
  const auto current_connections = client_fd_to_connection_->LookUp(client_fd);
  if (current_connections == std::nullopt) return 0;

  const auto channels_client_fds = channel_to_client_fds_->LookUp(channel);

  if (channels_client_fds == std::nullopt) return *current_connections->get();

  channels_client_fds->get()->Remove(client_fd);
  return --(*current_connections->get());
}
