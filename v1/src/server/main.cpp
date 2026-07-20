#include <iostream>
#include <string>

#include "ServerApp.h"

namespace {
void PrintUsage() {
  std::cout << "Usage: my_redis_server [OPTION...]\n"
               "  -l, --log    Log every request and response to stdout "
               "(slow; off by default)\n"
               "  -h, --help   Print usage\n";
}
}  // namespace

int main(const int argc, const char* const argv[]) {
  bool enable_request_logging = false;

  for (int arg = 1; arg < argc; ++arg) {
    const std::string option = argv[arg];
    if (option == "-l" || option == "--log") {
      enable_request_logging = true;
    } else if (option == "-h" || option == "--help") {
      PrintUsage();
      return 0;
    } else {
      std::cerr << "Unknown option: " << option << "\n";
      PrintUsage();
      return 1;
    }
  }

  const ServerApp app(enable_request_logging);
  return app.start();
}
