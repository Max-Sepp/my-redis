#ifndef MYREDIS_RESP_VALUE_RESP_VALUE_H_
#define MYREDIS_RESP_VALUE_RESP_VALUE_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace myredis {

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

  [[nodiscard]] std::string Serialize() const;
  [[nodiscard]] const RespVariant& GetValue() const;
  [[nodiscard]] std::string Show() const;

  static std::pair<RespValue, size_t> FromString(const std::string& str);
  static RespValue FromVariant(const RespVariant& variant);

 private:
  RespVariant value_;

  explicit RespValue(RespVariant variant);
  static RespVariant ParseVariant(const std::string& str, size_t& pos);
  static RespSimpleString ParseSimpleString(const std::string& str,
                                            size_t& pos);
  static RespSimpleError ParseSimpleError(const std::string& str, size_t& pos);
  static RespInteger ParseInteger(const std::string& str, size_t& pos);
  static RespBulkString ParseBulkString(const std::string& str, size_t& pos);
  static RespArray ParseArray(const std::string& str, size_t& pos);
};

}  // namespace myredis

#endif  // MYREDIS_RESP_VALUE_RESP_VALUE_H_
