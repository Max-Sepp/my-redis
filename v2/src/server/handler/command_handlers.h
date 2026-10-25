#ifndef MYREDIS_SERVER_HANDLER_COMMAND_HANDLERS_H_
#define MYREDIS_SERVER_HANDLER_COMMAND_HANDLERS_H_

#include "server/handler/del_request_handler.h"
#include "server/handler/echo_request_handler.h"
#include "server/handler/expire_request_handler.h"
#include "server/handler/get_request_handler.h"
#include "server/handler/hello_request_handler.h"
#include "server/handler/persist_request_handler.h"
#include "server/handler/ping_request_handler.h"
#include "server/handler/request_dispatcher.h"
#include "server/handler/set_request_handler.h"
#include "server/handler/ttl_request_handler.h"
#include "server/handler/unknown_request_handler.h"

namespace myredis {

inline constexpr int kSecondsToMilliseconds = 1000;

// The commands the server supports. Registering a handler here is all it takes
// to route its command; duplicate command names fail to compile.
// clang-format off
using CommandHandlers =
    HandlerRegistry<UnknownRequestHandler>
    ::Register<GetRequestHandler>
    ::Register<SetRequestHandler>
    ::Register<DelRequestHandler>
    ::Register<ExpireRequestHandler<"EXPIRE", kSecondsToMilliseconds, /*kRelativeToNow=*/true>>
    ::Register<ExpireRequestHandler<"PEXPIRE", 1, /*kRelativeToNow=*/true>>
    ::Register<ExpireRequestHandler<"EXPIREAT", kSecondsToMilliseconds, /*kRelativeToNow=*/false>>
    ::Register<ExpireRequestHandler<"PEXPIREAT", 1, /*kRelativeToNow=*/false>>
    ::Register<TtlRequestHandler<"TTL", kSecondsToMilliseconds>>
    ::Register<TtlRequestHandler<"PTTL", 1>>
    ::Register<PersistRequestHandler>
    ::Register<EchoRequestHandler>
    ::Register<PingRequestHandler>
    ::Register<HelloRequestHandler>;
// clang-format on

using CommandDispatcher = RequestDispatcher<CommandHandlers>;

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_COMMAND_HANDLERS_H_
