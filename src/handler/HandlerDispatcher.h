#ifndef MY_REDIS_HANDLER_H
#define MY_REDIS_HANDLER_H

#include <memory>
#include <vector>

#include "Handler.h"
#include "logger/Logger.h"
#include "respvalue/RespValue.h"

class HandlerDispatcher {
 public:
  HandlerDispatcher(std::vector<std::unique_ptr<Handler>> handlers,
                    const std::shared_ptr<Logger>& logger);

  void DispatchRequest(int client_fd, const RespValue& resp_value) const;

 private:
  std::vector<std::unique_ptr<Handler>> handlers_;
  std::shared_ptr<Logger> logger_;
};

#endif  // MY_REDIS_HANDLER_H
