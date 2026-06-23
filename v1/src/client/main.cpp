#include <unistd.h>

#include <cstdio>
#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <thread>

#include "Connect.h"
#include "Worker.h"

int main(const int argc, const char* argv[]) {
  cxxopts::Options options(
      "My Redis Client",
      "A basic redis client which is extend to work with my redis server.");

  options.add_options()(
      "h,hostname", "Hostname of the server",
      cxxopts::value<std::string>()->default_value("localhost"))(
      "p,port", "Server port", cxxopts::value<int>()->default_value("6379"));

  const auto result = options.parse(argc, argv);

  const std::string hostname = result["hostname"].as<std::string>();
  const int port = result["port"].as<int>();

  const int sock = Connect(hostname, std::to_string(port));
  if (sock == -1) {
    std::cerr << "Failed to connect to " << hostname << ':' << port << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "Successfully connected to: " << hostname << ":" << port << "\n";

  std::thread sender_thread(SenderWorker, sock);

  std::thread receiver_thread(ReceiverWorker, sock);

  sender_thread.join();
  receiver_thread.join();

  close(sock);

  return 0;
}
