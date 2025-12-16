#ifndef MY_REDIS_RESPVALUE_H
#define MY_REDIS_RESPVALUE_H

#include <optional>
#include <string>
#include <variant>
#include <vector>

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

  [[nodiscard]] std::string serialize() const;
  [[nodiscard]] const RespVariant& getValue() const;

  static std::pair<RespValue, size_t> FromString(const std::string& str);
  static RespValue FromVariant(const RespVariant& variant);

 private:
  RespVariant value_;

  explicit RespValue(RespVariant variant);
  static RespVariant parseVariant(const std::string& str, size_t& pos);
  static RespSimpleString parseSimpleString(const std::string& str,
                                            size_t& pos);
  static RespSimpleError parseSimpleError(const std::string& str, size_t& pos);
  static RespInteger parseInteger(const std::string& str, size_t& pos);
  static RespBulkString parseBulkString(const std::string& str, size_t& pos);
  static RespArray parseArray(const std::string& str, size_t& pos);
};

#endif  // MY_REDIS_RESPVALUE_H
