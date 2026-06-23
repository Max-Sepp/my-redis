#include "server/handler/request_dispatcher.h"

#include <memory>
#include <optional>
#include <string>

#include <utility>

#include "resp_value/resp_values.h"
#include "server/handler/del_request_handler.h"
#include "server/handler/get_request_handler.h"
#include "server/handler/set_request_handler.h"
#include "server/handler/unknown_request_handler.h"

namespace myredis {

RequestDispatcher::RequestDispatcher(
    std::unique_ptr<Map<std::string, std::optional<std::string>>> store)
    : store_(std::move(store)) {
  handlers_.push_back(std::make_unique<GetRequestHandler>(*store_));
  handlers_.push_back(std::make_unique<SetRequestHandler>(*store_));
  handlers_.push_back(std::make_unique<DelRequestHandler>(*store_));
  // Must remain last: matches anything the handlers above rejected.
  handlers_.push_back(std::make_unique<UnknownRequestHandler>());
}

RespValue RequestDispatcher::Dispatch(const RespValue& request) const {
  for (const auto& handler : handlers_) {
    if (handler->IsHandler(request)) {
      return handler->Handle(request);
    }
  }
  // Unreachable: UnknownRequestHandler always matches.
  return Error("Unknown subcommand or command");
}

}  // namespace myredis
