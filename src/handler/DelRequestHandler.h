#ifndef HANDLER_DELREQUESTHANDLER_H
#define HANDLER_DELREQUESTHANDLER_H

#include <memory>

#include "Handler.h"
#include "logger/Logger.h"
#include "store/Map.h"

class DelRequestHandler final : public Handler {
 public:
  DelRequestHandler(const std::shared_ptr<
                        Map<std::string, std::optional<std::string>>>& data_,
                    const std::shared_ptr<Logger>& logger_);

  [[nodiscard]] bool IsHandler(const RespValue& resp_value) const override;
  void Handle(int client_fd, const RespValue& resp_value) override;

 private:
  std::shared_ptr<Map<std::string, std::optional<std::string>>> data_;
  std::shared_ptr<Logger> logger_;
};

#endif  // HANDLER_DELREQUESTHANDLER_H