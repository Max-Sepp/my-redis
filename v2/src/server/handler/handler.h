#ifndef MYREDIS_SERVER_HANDLER_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_HANDLER_H_

#include "resp_value/resp_value.h"

namespace myredis {

// One link in the request-dispatch chain. The dispatcher walks its handlers in
// order, asking each whether it handles a given request; the first match
// produces the response.
//
// A handler returns the response RespValue rather than writing to a socket.
// Command execution is single-threaded, so handlers run without locking; the
// returned value is serialized and routed back to the owning IO thread.
class Handler {
 public:
  virtual ~Handler() = default;

  [[nodiscard]] virtual bool IsHandler(const RespValue& request) const = 0;
  [[nodiscard]] virtual RespValue Handle(const RespValue& request) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_HANDLER_H_
