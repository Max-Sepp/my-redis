#ifndef MYREDIS_RESP_VALUE_RESP_WRITER_H_
#define MYREDIS_RESP_VALUE_RESP_WRITER_H_

#include <cstddef>
#include <string_view>

#include "resp_value/byte_sink.h"

namespace myredis {

// Encodes RESP values straight into a ByteSink, so a reply can be produced
// without first building a RespValue. Each call appends one complete value,
// except ArrayHeader, which must be followed by exactly `size` element calls.
class RespWriter {
 public:
  explicit RespWriter(ByteSink& sink) : sink_(sink) {}

  void SimpleString(std::string_view string);
  void Error(std::string_view message);
  void Integer(long long num);
  void BulkString(std::string_view string);
  void NullBulkString();
  void ArrayHeader(std::size_t size);

 private:
  ByteSink& sink_;
};

}  // namespace myredis

#endif  // MYREDIS_RESP_VALUE_RESP_WRITER_H_
