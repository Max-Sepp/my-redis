#include <cxxopts.hpp>

#include "server/server.h"

int main(const int argc, const char* argv[]) {
  cxxopts::Options options(
      "My Redis Server",
      "A multi-threaded redis server: a single main thread "
      "executes commands while IO threads handle sockets.");

  options.add_options()("p,port", "Port to listen on",
                        cxxopts::value<int>()->default_value("6379"));
  options.add_options()("s,snapshot", "Interval between snapshots",
                        cxxopts::value<int>()->default_value("0"));

  const auto result = options.parse(argc, argv);
  const int port = result["port"].as<int>();
  const int snapshot_interval = result["snapshot"].as<int>();

  myredis::Server server(port, snapshot_interval);
  return server.Run();
}
