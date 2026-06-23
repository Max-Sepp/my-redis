cmake_minimum_required(VERSION 3.21)
project(my-redis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# --- Reusable building blocks carried over from v1 ---------------------------
# These packages are wrapped in `namespace myredis` and follow the Google C++
# Style Guide. The new server is being reimplemented on top of them.

set(RESP_VALUE_SOURCES
        src/resp_value/resp_value.cc
        src/resp_value/resp_value_queue.cc
        src/resp_value/resp_values.cc
)

set(NETWORK_SOURCES
        src/network/send_all.cc
)

set(CLIENT_SOURCES
        ${RESP_VALUE_SOURCES}
        ${NETWORK_SOURCES}
        src/client/connect.cc
        src/client/string_helpers.cc
        src/client/worker.cc
)

# --- Client executable -------------------------------------------------------
add_executable(my_redis_client src/client/main.cc ${CLIENT_SOURCES})
target_include_directories(my_redis_client PRIVATE src)
target_include_directories(my_redis_client PRIVATE thirdparty)

# --- Server executable -------------------------------------------------------
# TODO(v2): reimplement the server here. It will reuse RESP_VALUE_SOURCES,
# NETWORK_SOURCES and src/concurrent/concurrent_queue.h (header-only).
