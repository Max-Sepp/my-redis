#ifndef MYREDIS_SERVER_CONNECTION_H_
#define MYREDIS_SERVER_CONNECTION_H_

#include <string>

#include "resp_value/resp_value_queue.h"

namespace myredis {

// Per-client state owned exclusively by the single IO thread that the client
// was assigned to. Because exactly one thread touches a Connection, it needs no
// locking (unlike v1's ClientConnection, which was shared across a worker pool
// and guarded by a mutex).
struct Connection {
  explicit Connection(int client_fd) : fd(client_fd) {}

  int fd;
  // Incrementally accumulates received bytes and yields parsed RESP values.
  RespValueQueue parse_queue;
  // Bytes queued for writing that have not yet been accepted by the socket
  // (i.e. SendAll returned would-block). Drained on EPOLLOUT.
  std::string out_buffer;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_CONNECTION_H_
