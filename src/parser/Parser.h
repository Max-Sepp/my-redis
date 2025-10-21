#ifndef MY_REDIS_PARSER_H
#define MY_REDIS_PARSER_H

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

class RespValue {
 public:
  using RespSimpleString = std::string;
  struct RespSimpleError {
    std::string message;
  };
  using RespInteger = int64_t;
  using RespBulkString = std::optional<std::string>;
  using RespArray = std::vector<RespValue>;
  using RespVariant = std::variant<RespSimpleString, RespSimpleError,
                                   RespInteger, RespBulkString, RespArray>;

  explicit RespValue(const std::string& resp_string);
  explicit RespValue(RespVariant variant) : value(std::move(variant)) {}

  [[nodiscard]] std::string serialize() const;

  [[nodiscard]] const RespVariant& getValue() const;

 private:
  RespVariant value;

  static RespVariant ParseValue(const std::string& str, size_t& pos);
  static RespSimpleString ParseSimpleString(const std::string& str,
                                            size_t& pos);
  static RespSimpleError ParseSimpleError(const std::string& str, size_t& pos);
  static RespInteger ParseInteger(const std::string& str, size_t& pos);
  static RespBulkString ParseBulkString(const std::string& str, size_t& pos);
  static RespArray ParseArray(const std::string& str, size_t& pos);
};

#endif  // MY_REDIS_PARSER_H
