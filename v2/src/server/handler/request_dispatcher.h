#ifndef MYREDIS_SERVER_HANDLER_REQUEST_DISPATCHER_H_
#define MYREDIS_SERVER_HANDLER_REQUEST_DISPATCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "resp_value/resp_value.h"
#include "server/handler/handler.h"
#include "store/map.h"

namespace myredis {

// Routes a parsed request to the first handler that claims it and returns the
// response (ported from v1's HandlerDispatcher). Takes ownership of the command
// store, which the handlers reference.
//
// The store is injected by the main thread (which picks the concrete Map
// implementation) rather than created here, so the dispatcher stays decoupled
// from any particular store.
//
// Single-threaded: the server's main thread is the sole caller of Dispatch, so
// the store needs no locking. Not thread-safe by design.
class RequestDispatcher {
 public:
  explicit RequestDispatcher(
      std::unique_ptr<Map<std::string, std::optional<std::string>>> store);

  RequestDispatcher(const RequestDispatcher&) = delete;
  RequestDispatcher& operator=(const RequestDispatcher&) = delete;

  // Executes `request` and returns the response. The chain always ends in an
  // UnknownRequestHandler, so this always produces a value.
  [[nodiscard]] RespValue Dispatch(const RespValue& request) const;

 private:
  // Declared before `handlers_` so it outlives the handlers that reference it.
  std::unique_ptr<Map<std::string, std::optional<std::string>>> store_;
  std::vector<std::unique_ptr<Handler>> handlers_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_REQUEST_DISPATCHER_H_
