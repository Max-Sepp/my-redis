#ifndef MYREDIS_SERVER_HANDLER_HANDLER_H_
#define MYREDIS_SERVER_HANDLER_HANDLER_H_

#include "resp_value/resp_value.h"
#include "resp_value/resp_writer.h"

namespace myredis {

// One link in the request-dispatch chain. The dispatcher walks its handlers in
// order, asking each whether it handles a given request; the first match
// produces the response.
//
// A handler encodes its response into `out` rather than writing to a socket or
// building a RespValue. Command execution is single-threaded, so handlers run
// without locking; `out` streams the bytes back to the owning IO thread.
class Handler {
 public:
  virtual ~Handler() = default;

  [[nodiscard]] virtual bool IsHandler(const RespValue& request) const = 0;
  virtual void Handle(const RespValue& request, RespWriter& out) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_HANDLER_H_
