#ifndef MYREDIS_SERVER_SERVER_H_
#define MYREDIS_SERVER_SERVER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "concurrent/event_fd.h"
#include "server/io_thread.h"
#include "server/messages.h"

namespace myredis {

// The server's main thread. It owns the listening socket and is the single
// command executor: IO threads parse client bytes into RESP requests and hand
// them here, the main thread executes each one (single-threaded, so the store
// will need no locking) and routes the response bytes back to the IO thread
// that owns the client.
//
// The main thread runs one epoll loop watching the listen socket (for new
// connections) and a shared command eventfd (signalled by the IO threads when
// they enqueue work).
class Server {
 public:
  explicit Server(int port);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  // Starts the IO threads and runs the main loop. Returns a process exit code.
  int Run();

 private:
  void AcceptConnections();
  void AssignToIoThread(int client_fd);
  void ProcessCommands();
  void ExecuteAndRespond(const Command& command);

  // Executes a single request and returns the serialized response bytes.
  // STUB: currently echoes the request back. Real command dispatch and the
  // single-threaded store come in a later step.
  std::string Execute(const RespValue& request);

  int epoll_fd_ = -1;
  int listen_fd_ = -1;
  EventFd command_event_;  // IO threads -> main wakeup; shared by all IO threads

  std::vector<std::unique_ptr<IoThread>> io_threads_;
  // Maps a client fd to the index of the IO thread that owns it. Touched only
  // by the main thread (populated on accept, erased on Disconnect). Needed to
  // route a response to the owning thread even when the executed command
  // targets a different client (e.g. future pub/sub fan-out).
  std::unordered_map<int, int> fd_to_thread_;
  std::size_t next_thread_ = 0;  // round-robin assignment cursor
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_SERVER_H_
