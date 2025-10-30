#ifndef MY_REDIS_PARSER_H
#define MY_REDIS_PARSER_H

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "RecvBufferIterator.h"

class RespValue {
 public:
  struct RespSimpleError {
    std::string message;
  };

  using RespSimpleString = std::string;
  using RespInteger = long long;
  using RespBulkString = std::optional<std::string>;
  using RespArray = std::vector<RespValue>;
  using RespVariant = std::variant<RespSimpleString, RespSimpleError,
                                   RespInteger, RespBulkString, RespArray>;

  RespValue(RecvBufferIterator& begin, RecvBufferIterator& end);
  explicit RespValue(const std::string& resp_string);
  explicit RespValue(RespVariant variant) : value(std::move(variant)) {}

  [[nodiscard]] std::string serialize() const;
  [[nodiscard]] const RespVariant& getValue() const;

 private:
  RespVariant value;
};

#endif  // MY_REDIS_PARSER_H
