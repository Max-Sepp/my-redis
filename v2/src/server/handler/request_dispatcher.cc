#include "server/handler/request_dispatcher.h"

#include <memory>
#include <string>

#include "resp_value/resp_values.h"
#include "server/handler/del_request_handler.h"
#include "server/handler/echo_request_handler.h"
#include "server/handler/get_request_handler.h"
#include "server/handler/hello_request_handler.h"
#include "server/handler/ping_request_handler.h"
#include "server/handler/set_request_handler.h"
#include "server/handler/unknown_request_handler.h"

namespace myredis {

RequestDispatcher::RequestDispatcher(const std::unique_ptr<Store>& store)
    : store_(store) {
  handlers_.push_back(std::make_unique<GetRequestHandler>(store_));
  handlers_.push_back(std::make_unique<SetRequestHandler>(store_));
  handlers_.push_back(std::make_unique<DelRequestHandler>(store_));
  handlers_.push_back(std::make_unique<EchoRequestHandler>());
  handlers_.push_back(std::make_unique<PingRequestHandler>());
  handlers_.push_back(std::make_unique<HelloRequestHandler>());
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
