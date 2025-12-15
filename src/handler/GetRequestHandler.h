#ifndef HANDLER_GETREQUESTHANDLER_H
#define HANDLER_GETREQUESTHANDLER_H

#include "Handler.h"

class GetRequestHandler final : public Handler {
 public:
  GetRequestHandler(const std::shared_ptr<
                        Map<std::string, std::optional<std::string>>>& data_,
                    const std::shared_ptr<Logger>& logger_);

  [[nodiscard]] bool IsHandler(const RespValue& resp_value) const override;
  void Handle(int client_fd, const RespValue& resp_value) override;
  [[nodiscard]] static std::unique_ptr<Handler> CreateHandler(
      const std::shared_ptr<Map<std::string, std::optional<std::string>>>& data,
      const std::shared_ptr<Logger>& logger);

 private:
  std::shared_ptr<Map<std::string, std::optional<std::string>>> data_;
  std::shared_ptr<Logger> logger_;
};

#endif  // HANDLER_GETREQUESTHANDLER_H