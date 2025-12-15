#ifndef HANDLER_HANDLER_H
#define HANDLER_HANDLER_H

#include <memory>

#include "logger/Logger.h"
#include "respvalue/RespValue.h"
#include "store/Map.h"

class Handler {
 public:
  virtual ~Handler() = default;
  [[nodiscard]] virtual bool IsHandler(const RespValue& resp_value) const = 0;
  virtual void Handle(int client_fd, const RespValue& resp_value) = 0;
  [[nodiscard]] static std::unique_ptr<Handler> CreateHandler(
      const std::shared_ptr<Map<std::string, std::optional<std::string>>>&
          store,
      const std::shared_ptr<Logger>& logger);
};

#endif  // HANDLER_HANDLER_H
