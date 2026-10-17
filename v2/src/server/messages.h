#ifndef MYREDIS_SERVER_MESSAGES_H_
#define MYREDIS_SERVER_MESSAGES_H_

#include <cstdint>
#include <variant>
#include <vector>

#include "resp_value/resp_value.h"

namespace myredis {

// Messages flowing main -> IO thread. Each is a record in the IO thread's
// `inbox_` byte ring: an InboxHeader followed by `length` payload bytes.

enum class InboxRecordType : std::uint8_t {
  // The main thread accepted a new client and assigned it to this IO thread,
  // which should take ownership of the fd (epoll ADD + create Connection
  // state). No payload.
  kAssignConnection,
  // Response bytes for a client owned by this IO thread, which should buffer
  // and write the payload to `fd`. A long response may span several records.
  kWriteResponse,
};

struct InboxHeader {
  InboxRecordType type;
  int fd;
  std::uint32_t length;  // payload bytes following the header
};

// Messages flowing IO thread -> main thread (carried on the IO thread's
// `outbox_`).

// A batch of fully-parsed RESP requests from a single client, all drained from
// one read. The main thread executes them in order and returns one coalesced
// response. Batching amortises the cross-thread handoff: a pipelined burst of N
// commands costs one outbox push + wakeup instead of N (and one PostResponse
// back instead of N), which is what makes single-connection pipelining fast.
struct CommandBatch {
  int fd = -1;
  std::vector<RespValue> values;
};

// The client closed (or errored) and the IO thread has already closed the fd;
// the main thread should drop its fd -> thread routing entry.
struct Disconnect {
  int fd = -1;
};

using OutboxMsg = std::variant<CommandBatch, Disconnect>;

}  // namespace myredis

#endif  // MYREDIS_SERVER_MESSAGES_H_
