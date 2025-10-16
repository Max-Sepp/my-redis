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

  explicit RespValue(const std::string& respString);
  explicit RespValue(RespVariant variant) : value(std::move(variant)) {}

  [[nodiscard]] std::string serialize() const;

  [[nodiscard]] const RespVariant& getValue() const;

 private:
  RespVariant value;

  static RespVariant parseValue(const std::string& str, size_t& pos);
  static RespSimpleString parseSimpleString(const std::string& str,
                                            size_t& pos);
  static RespSimpleError parseSimpleError(const std::string& str, size_t& pos);
  static RespInteger parseInteger(const std::string& str, size_t& pos);
  static RespBulkString parseBulkString(const std::string& str, size_t& pos);
  static RespArray parseArray(const std::string& str, size_t& pos);
};

#endif  // MY_REDIS_PARSER_H
