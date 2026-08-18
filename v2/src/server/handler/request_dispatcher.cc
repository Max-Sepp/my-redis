#include "server/handler/request_dispatcher.h"

#include <memory>
#include <string>

#include "resp_value/resp_values.h"
#include "server/handler/del_request_handler.h"
#include "server/handler/echo_request_handler.h"
#include "server/handler/expire_request_handler.h"
#include "server/handler/get_request_handler.h"
#include "server/handler/hello_request_handler.h"
#include "server/handler/persist_request_handler.h"
#include "server/handler/ping_request_handler.h"
#include "server/handler/set_request_handler.h"
#include "server/handler/ttl_request_handler.h"
#include "server/handler/unknown_request_handler.h"

namespace {
constexpr int SECONDS_TO_MILLISECONDS = 1000;
}

namespace myredis {

RequestDispatcher::RequestDispatcher(const std::unique_ptr<Store>& store)
    : store_(store) {
  handlers_.push_back(std::make_unique<GetRequestHandler>(store_));
  handlers_.push_back(std::make_unique<SetRequestHandler>(store_));
  handlers_.push_back(std::make_unique<DelRequestHandler>(store_));
  // EXPIRE handler
  handlers_.push_back(std::make_unique<ExpireRequestHandler>(
      "EXPIRE", SECONDS_TO_MILLISECONDS, /*relative_to_now=*/true, store_));
  // PEXPIRE handler
  handlers_.push_back(std::make_unique<ExpireRequestHandler>(
      "PEXPIRE", 1, /*relative_to_now=*/true, store_));
  // EXPIREAT handler
  handlers_.push_back(std::make_unique<ExpireRequestHandler>(
      "EXPIREAT", SECONDS_TO_MILLISECONDS, /*relative_to_now=*/false,
      store_));
  // PEXPIREAT handler
  handlers_.push_back(std::make_unique<ExpireRequestHandler>(
      "PEXPIREAT", 1, /*relative_to_now=*/false, store_));
  // TTL
  handlers_.push_back(std::make_unique<TtlRequestHandler>(
      "TTL", SECONDS_TO_MILLISECONDS, store_));
  // PTTL
  handlers_.push_back(std::make_unique<TtlRequestHandler>("PTTL", 1, store_));
  // PERSIST
  handlers_.push_back(std::make_unique<PersistRequestHandler>(store_));
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
