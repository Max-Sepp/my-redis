#ifndef MYREDIS_CONCURRENT_CACHE_LINE_H_
#define MYREDIS_CONCURRENT_CACHE_LINE_H_

#include <cstddef>
#include <new>

namespace myredis {

// libstdc++ only provides std::hardware_destructive_interference_size when the
// compiler defines __GCC_DESTRUCTIVE_SIZE (Clang < 19 does not), so fall back
// to 64 bytes, the cache line size on x86-64 and most ARM cores.
#ifdef __cpp_lib_hardware_interference_size
inline constexpr std::size_t kCacheLineSize =
    std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t kCacheLineSize = 64;
#endif

}  // namespace myredis

#endif  // MYREDIS_CONCURRENT_CACHE_LINE_H_
