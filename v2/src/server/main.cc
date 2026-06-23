#include <cxxopts.hpp>

#include "server/server.h"

int main(const int argc, const char* argv[]) {
  cxxopts::Options options("My Redis Server",
                           "A multi-threaded redis server: a single main thread "
                           "executes commands while IO threads handle sockets.");

  options.add_options()("p,port", "Port to listen on",
                        cxxopts::value<int>()->default_value("6379"));

  const auto result = options.parse(argc, argv);
  const int port = result["port"].as<int>();

  myredis::Server server(port);
  return server.Run();
}
