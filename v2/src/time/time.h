#ifndef MYREDIS_TIME_TIME_H_
#define MYREDIS_TIME_TIME_H_

#include <cstdint>

namespace myredis {

class Time {
 public:
  virtual std::int64_t NowMs() = 0;

  virtual ~Time() = default;
};

}  // namespace myredis

#endif  // MYREDIS_TIME_TIME_H_
