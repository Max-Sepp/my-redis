#include "worker.h"

#include <sys/socket.h>

#include <array>
#include <iostream>
#include <string>

#include "network/send_all.h"
#include "resp_value/resp_value.h"
#include "resp_value/resp_value_queue.h"
#include "resp_value/resp_values.h"
#include "string_helpers.h"

namespace myredis {

constexpr int kBufferSize = 1024;

void SenderWorker(const int sock) {
  while (true) {
    std::string input;
    std::getline(std::cin, input);

    if (input == std::string("exit")) return;

    std::vector<std::string> split_input =
        JoinDelimitedStrings(Split(" ", input));

    std::vector<RespValue> resp_values;
    resp_values.reserve(split_input.size());
    for (const std::string& value : split_input) {
      resp_values.push_back(BulkString(value));
    }
    RespValue message = Array(resp_values);
    std::string serialised_message = message.Serialize();

    if (SendAll(sock, serialised_message.c_str(), serialised_message.size(),
                0) != serialised_message.size()) {
      // SendAll was not successful
      return;
    }
  }
}

void ReceiverWorker(const int sock) {
  RespValueQueue resp_value_queue;
  std::array<char, kBufferSize> buffer{};
  while (true) {
    if (const ssize_t num_bytes = recv(sock, buffer.data(), buffer.size(), 0);
        num_bytes > 0) {
      resp_value_queue.PushString(std::string(buffer.data(), 0, num_bytes));
    } else if (num_bytes == 0) {
      break;
    } else if (errno != EINTR) {
      perror("recv");
      break;
    }

    std::optional<RespValue> maybe_value = resp_value_queue.PopValue();
    while (maybe_value != std::nullopt) {
      const RespValue& value = maybe_value.value();
      std::cout << value.Show() << "\n";

      maybe_value = resp_value_queue.PopValue();
    }
  }
}

}  // namespace myredis
