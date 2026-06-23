#include "connect.h"

#include <netdb.h>
#include <unistd.h>

#include <iostream>

namespace myredis {

int Connect(const std::string& hostname, const std::string& port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;

  addrinfo* res = nullptr;
  const int getaddrinfo_result =
      getaddrinfo(hostname.c_str(), port.c_str(), &hints, &res);
  if (getaddrinfo_result != 0) {
    std::cerr << "getaddrinfo('" << hostname << "', '" << port
              << "'): " << gai_strerror(getaddrinfo_result) << "\n";
    return -1;
  }

  int sock = -1;
  for (const addrinfo* resolved_address = res; resolved_address != nullptr;
       resolved_address = resolved_address->ai_next) {
    sock = socket(resolved_address->ai_family, resolved_address->ai_socktype,
                  resolved_address->ai_protocol);
    if (sock == -1) {
      std::perror("socket");
      continue;  // try next addrinfo
    }

    if (connect(sock, resolved_address->ai_addr,
                resolved_address->ai_addrlen) == 0) {
      // success
      break;
    }
    close(sock);
    sock = -1;
  }

  freeaddrinfo(res);
  return sock;  // -1 on failure, >=0 on success
}

}  // namespace myredis
