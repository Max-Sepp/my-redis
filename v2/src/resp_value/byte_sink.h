#ifndef MYREDIS_RESP_VALUE_BYTE_SINK_H_
#define MYREDIS_RESP_VALUE_BYTE_SINK_H_

#include <string_view>

namespace myredis {

// Destination for serialized bytes, so callers choose where they land.
class ByteSink {
 public:
  virtual ~ByteSink() = default;
  virtual void Append(std::string_view bytes) = 0;
};

}  // namespace myredis

#endif  // MYREDIS_RESP_VALUE_BYTE_SINK_H_
