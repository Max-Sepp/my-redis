#ifndef MYREDIS_TIME_TIMENOW_H_
#define MYREDIS_TIME_TIMENOW_H_

#include <chrono>
#include <cstdint>

#include "time/time.h"

namespace myredis {

class TimeNow final : public Time {
 public:
  std::int64_t NowMs() override {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
  }
};

}  // namespace myredis

#endif  // MYREDIS_TIME_TIMENOW_H_
