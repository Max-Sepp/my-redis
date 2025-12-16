#ifndef HANDLER_UNKNOWNREQUESTHANDLER_H
#define HANDLER_UNKNOWNREQUESTHANDLER_H

#include <memory>

#include "Handler.h"
#include "logger/Logger.h"
#include "store/Map.h"

class UnknownRequestHandler final : public Handler {
 public:
  UnknownRequestHandler(
      const std::shared_ptr<Map<std::string, std::optional<std::string>>>&
          store,
      const std::shared_ptr<Logger>& logger);

  [[nodiscard]] bool IsHandler(const RespValue& resp_value) const override;
  void Handle(int client_fd, const RespValue& resp_value) override;
  [[nodiscard]] static std::unique_ptr<Handler> CreateHandler(
      const std::shared_ptr<Map<std::string, std::optional<std::string>>>&
          store,
      const std::shared_ptr<Logger>& logger);

 private:
  std::shared_ptr<Logger> logger_;
};

#endif  // HANDLER_UNKNOWNREQUESTHANDLER_H