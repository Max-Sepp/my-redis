#ifndef MYREDIS_SERVER_CONNECTION_H_
#define MYREDIS_SERVER_CONNECTION_H_

#include <string>
#include <string_view>
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
// interest) is up to the caller. The caller also owns the fd's lifetime.
class Connection {
 public:
  struct ReadResult {
    // Every request fully parsed from this read, oldest first. Empty if the
    // read hit malformed RESP framing.
    std::vector<RespValue> requests;
    // False if the connection should be closed: peer shutdown, a fatal socket
    // error, or malformed RESP framing.
    bool keep_open;
  };

  explicit Connection(int client_fd) : fd_(client_fd) {}

  // Reads until the socket would block and returns the requests parsed so far.
  ReadResult Read();

  // Queues bytes for writing and immediately flushes as much as the socket
  // accepts. Returns false on a fatal write error, after which the connection
  // should be closed.
  bool Send(std::string_view bytes);

  // Writes as much of the pending bytes as the socket accepts; whatever
  // remains should be retried on EPOLLOUT. Returns false on a fatal write
  // error, after which the connection should be closed.
  bool Flush();

  // True while bytes remain that the socket has not yet accepted.
  [[nodiscard]] bool HasPendingWrites() const { return !out_buffer_.empty(); }

 private:
  int fd_;
  // Incrementally accumulates received bytes and yields parsed RESP values.
  RespValueQueue parse_queue_;
  // Bytes queued for writing that have not yet been accepted by the socket.
  // Drained on EPOLLOUT.
  std::string out_buffer_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_CONNECTION_H_
