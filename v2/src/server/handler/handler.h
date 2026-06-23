#ifndef MYREDIS_SERVER_HANDLER_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_HANDLER_H_

#include "resp_value/resp_value.h"

namespace myredis {

// One link in the request-dispatch chain (ported from v1). The dispatcher walks
// its handlers in order, asking each whether it handles a given request; the
// first match produces the response.
//
// Unlike v1 -- where handlers wrote directly to the client socket -- a v2
// handler returns the response RespValue. The main thread is the single command
// executor, so handlers run without locking and never touch a socket: the
// returned value is serialized and routed back to the owning IO thread.
class Handler {
 public:
  virtual ~Handler() = default;

  [[nodiscard]] virtual bool IsHandler(const RespValue& request) const = 0;
  [[nodiscard]] virtual RespValue Handle(const RespValue& request) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_HANDLER_H_
