#ifndef MYREDIS_SERVER_HANDLER_REQUEST_DISPATCHER_H_
#define MYREDIS_SERVER_HANDLER_REQUEST_DISPATCHER_H_

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"
#include "server/handler/command.h"
#include "server/handler/command_table.h"
#include "server/handler/handler.h"
#include "store/store.h"

namespace myredis {

// A compile-time list of handlers, built up with Register:
//
//   using Handlers = HandlerRegistry<UnknownRequestHandler>
//                        ::Register<GetRequestHandler>
//                        ::Register<SetRequestHandler>;
//
// `Fallback` answers requests no registered handler takes.
template <FallbackHandler Fallback, RequestHandler... Handlers>
struct HandlerRegistry {
  template <RequestHandler H>
  using Register = HandlerRegistry<Fallback, Handlers..., H>;
};

// Routes a parsed request to the registered handler for its command keyword,
// which writes the response. The keyword is resolved by a perfect hash over
// the handlers' kNames built at compile time (see command_table.h), and the
// handler is invoked through a table of function pointers indexed by the
// result: no walk over the handlers and no virtual calls.
//
// Handlers are constructed from the injected store if they take one, else
// default-constructed; this keeps the dispatcher decoupled from any concrete
// handler or Map.
//
// Single-threaded: the sole caller of Dispatch is the server's main thread, so
// the store needs no locking. Not thread-safe by design.
template <typename Registry>
class RequestDispatcher;

template <typename Fallback, typename... Handlers>
class RequestDispatcher<HandlerRegistry<Fallback, Handlers...>> {
  static_assert(CommandTable<sizeof...(Handlers)>::AreUnique(
                    {std::string_view(Handlers::kName)...}),
                "two registered handlers share a command name");

 public:
  explicit RequestDispatcher(const std::unique_ptr<Store>& store)
      : handlers_(Construct<Handlers>(store)...),
        fallback_(Construct<Fallback>(store)) {}

  RequestDispatcher(const RequestDispatcher&) = delete;
  RequestDispatcher& operator=(const RequestDispatcher&) = delete;

  // Executes `request` and writes exactly one response value to `out`.
  // Unrecognised commands, and recognised ones their handler rejects, go to
  // the fallback handler.
  void Dispatch(const RespValue& request, RespWriter& out) {
    using Invoker = void (*)(RequestDispatcher&, const RespValue&, RespWriter&);
    // One entry per handler, then the fallback at kNotFound.
    static constexpr std::array<Invoker, sizeof...(Handlers) + 1> kJumpTable =
        []<std::size_t... Is>(std::index_sequence<Is...> /*indices*/) {
          return std::array<Invoker, sizeof...(Handlers) + 1>{&Invoke<Is>...,
                                                              &InvokeFallback};
        }(std::index_sequence_for<Handlers...>{});

    const std::optional<std::string_view> name = PeekCommandName(request);
    const std::size_t index =
        name ? kCommandTable.Find(*name) : decltype(kCommandTable)::kNotFound;
    kJumpTable[index](*this, request, out);
  }

 private:
  static constexpr CommandTable<sizeof...(Handlers)> kCommandTable{
      std::array<std::string_view, sizeof...(Handlers)>{Handlers::kName...}};

  template <typename H>
  static H Construct(const std::unique_ptr<Store>& store) {
    if constexpr (std::is_constructible_v<H, const std::unique_ptr<Store>&>) {
      return H(store);
    } else {
      return H();
    }
  }

  // Runs handler I if it accepts `request`, else the fallback.
  template <std::size_t I>
  static void Invoke(RequestDispatcher& self, const RespValue& request,
                     RespWriter& out) {
    auto& handler = std::get<I>(self.handlers_);
    if (handler.IsHandler(request)) {
      handler.Handle(request, out);
    } else {
      self.fallback_.Handle(request, out);
    }
  }

  static void InvokeFallback(RequestDispatcher& self, const RespValue& request,
                             RespWriter& out) {
    self.fallback_.Handle(request, out);
  }

  std::tuple<Handlers...> handlers_;
  Fallback fallback_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_REQUEST_DISPATCHER_H_
