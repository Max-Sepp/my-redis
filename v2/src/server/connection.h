#ifndef MYREDIS_SERVER_CONNECTION_H_
#define MYREDIS_SERVER_CONNECTION_H_

#include <string>
#include <vector>

#include "resp_value/resp_value.h"
#include "resp_value/resp_value_queue.h"

namespace myredis {

// Per-client state owned exclusively by the single IO thread that the client
// was assigned to. Because exactly one thread touches a Connection, it needs no
// locking.
//
// The socket IO methods only touch the Connection; what to do with the result
// (closing the connection, handing requests to the main thread, epoll
// interest) is up to the caller.
struct Connection {
  explicit Connection(int client_fd) : fd(client_fd) {}

  // Reads until the socket would block, appending the bytes to parse_queue.
  // Returns false if the connection should be closed (peer shutdown or fatal
  // error), true if it is still alive.
  bool ReadIntoParseQueue();

  // Pops every fully parsed request out of parse_queue, oldest first. Throws
  // std::invalid_argument on malformed RESP framing.
  std::vector<RespValue> TakeParsedRequests();

  // Writes as much of out_buffer as the socket accepts and drops the bytes
  // that were sent; whatever remains should be retried on EPOLLOUT. Returns
  // false on a fatal write error, after which the connection should be
  // closed.
  bool WriteOutBuffer();

  int fd;
  // Incrementally accumulates received bytes and yields parsed RESP values.
  RespValueQueue parse_queue;
  // Bytes queued for writing that have not yet been accepted by the socket
  // (i.e. SendAll returned would-block). Drained on EPOLLOUT.
  std::string out_buffer;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_CONNECTION_H_
