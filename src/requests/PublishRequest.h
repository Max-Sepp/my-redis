#ifndef MY_REDIS_PUBLISHREQUEST_H
#define MY_REDIS_PUBLISHREQUEST_H
#include <string>

#include "respvalue/RespValue.h"

class PublishRequest {
 public:
  explicit PublishRequest(const RespValue& resp_value);
  [[nodiscard]] const std::string& getChannel() const;
  [[nodiscard]] const std::string& getMessage() const;
  [[nodiscard]] static bool IsRequest(const RespValue& resp_value);

 private:
  std::string channel_;
  std::string message_;
};

#endif  // MY_REDIS_PUBLISHREQUEST_H
