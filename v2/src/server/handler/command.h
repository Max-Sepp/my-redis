#ifndef MYREDIS_SERVER_HANDLER_COMMAND_H_
#define MYREDIS_SERVER_HANDLER_COMMAND_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "resp_value/resp_value.h"

namespace myredis {

// A client request is a RESP array of bulk strings: the command keyword
// followed by its arguments. `Command` is the parsed form shared by the
// handlers in the request-dispatch chain.
//
// `name` is the (non-null, non-empty) command keyword. `args` are the bulk
// strings that follow it; an argument may itself be a null bulk string (e.g.
// the value of `SET key <nil>`), hence std::optional.
struct Command {
  std::string name;
  std::vector<std::optional<std::string>> args;
};

// Parses `request` into a Command, or returns std::nullopt if it is not a
// well-formed command array (not an array, empty, or a non-bulk-string /
// null / empty command keyword).
inline std::optional<Command> ParseCommand(const RespValue& request) {
  const auto* array =
      std::get_if<RespValue::RespArray>(&request.GetValue());
  if (array == nullptr || array->empty()) return std::nullopt;

  const auto* name = std::get_if<RespValue::RespBulkString>(
      &array->front().GetValue());
  if (name == nullptr || !name->has_value() || (*name)->empty()) {
    return std::nullopt;
  }

  Command command;
  command.name = **name;
  command.args.reserve(array->size() - 1);
  for (std::size_t i = 1; i < array->size(); ++i) {
    const auto* arg =
        std::get_if<RespValue::RespBulkString>(&(*array)[i].GetValue());
    if (arg == nullptr) return std::nullopt;  // arguments must be bulk strings
    command.args.push_back(*arg);
  }
  return command;
}

}  // namespace myredis

#endif  // MYREDIS_SERVER_HANDLER_COMMAND_H_
