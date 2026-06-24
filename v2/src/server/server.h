#ifndef MYREDIS_SERVER_SERVER_H_
#define MYREDIS_SERVER_SERVER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "concurrent/event_fd.h"
#include "server/handler/request_dispatcher.h"
#include "server/io_thread.h"
#include "server/messages.h"
#include "snapshot/snapshotter.h"

namespace myredis {

struct ServerConfig {
  int port;
  // Milliseconds between snapshots; <= 0 disables snapshotting.
  int snapshot_interval_ms;
};

// The server's main thread. It owns the listening socket and is the single
// command executor: IO threads parse client bytes into RESP requests and hand
// them here, the main thread executes each one (single-threaded, so the store
// needs no locking) and routes the response bytes back to the IO thread that
// owns the client.
//
// The main thread runs one epoll loop watching the listen socket (for new
// connections) and a shared command eventfd (signalled by the IO threads when
// they enqueue work).
class Server {
 public:
  explicit Server(ServerConfig config);
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
  void CreateSnapshot();
  // Reaps a finished snapshot child (identified by its pidfd) and stops
  // watching it.
  void ReapSnapshot(int pidfd);

  // Executes a single request via the dispatcher and returns the serialized
  // response bytes.
  std::string Execute(const RespValue& request);

  // The store the dispatcher and its handlers reference. Declared before
  // `dispatcher_` so it is constructed first: the dispatcher binds a reference
  // to this handle during its own construction.
  std::unique_ptr<Map<std::string, std::optional<std::string>>> store_;

  // Single command executor: the main thread owns the dispatcher and its store,
  // so command execution needs no locking.
  RequestDispatcher dispatcher_;

  // Periodically forks to write the store to disk; see CreateSnapshot.
  Snapshotter snapshotter_;

  int epoll_fd_ = -1;
  int listen_fd_ = -1;
  int snapshot_fd_ = -1;
  // IO threads -> main wakeup; shared by all IO threads
  EventFd command_event_;

  std::vector<std::unique_ptr<IoThread>> io_threads_;
  // Maps a client fd to the index of the IO thread that owns it. Touched only
  // by the main thread (populated on accept, erased on Disconnect). Needed to
  // route a response to the owning thread even when the executed command
  // targets a different client.
  std::unordered_map<int, int> fd_to_thread_;
  std::size_t next_thread_ = 0;  // round-robin assignment cursor

  // Maps a snapshot child's pidfd to its pid so the main thread can reap the
  // child (and remove the pidfd from epoll) once it exits.
  std::unordered_map<int, int> snapshot_children_;
};

}  // namespace myredis

#endif  // MYREDIS_SERVER_SERVER_H_
