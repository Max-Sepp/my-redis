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
# A single main thread executes commands; a pool of IO threads handle sockets
# and RESP parsing. Reuses RESP_VALUE_SOURCES plus the header-only concurrent
# queues; FlushOutBuffer does its own partial-write tracking so NETWORK_SOURCES
# (SendAll) is not needed here.

set(SERVER_SOURCES
        ${RESP_VALUE_SOURCES}
        src/concurrent/event_fd.cc
        src/server/handler/request_dispatcher.cc
        src/server/io_thread.cc
        src/server/server.cc
)

add_executable(my_redis_server src/server/main.cc ${SERVER_SOURCES})
target_include_directories(my_redis_server PRIVATE src)
target_include_directories(my_redis_server PRIVATE thirdparty)

# --- Testing -----------------------------------------------------------------
# Only include tests if the TESTS option is ON (default: OFF)
option(BUILD_TESTS "Build the tests" OFF)

# Automatically enable tests for Debug builds unless the user explicitly disabled them.
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT BUILD_TESTS)
    set(BUILD_TESTS ON CACHE BOOL "Build the tests" FORCE)
endif ()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "BUILD_TESTS = ${BUILD_TESTS}")

if (BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # Add the test executable with its own sources plus the library sources
    add_executable(parser_tests
            tests/parser_tests.cc
            src/resp_value/resp_value.cc
    )

    # Link the test executable against Google Test
    target_link_libraries(parser_tests PRIVATE GTest::gtest_main)

    # Include directories for the test executable
    target_include_directories(parser_tests PRIVATE src)

    # Store tests: the non-concurrent maps are header-only and depend on the
    # store/hash.cc helper.
    add_executable(store_tests
            tests/store_tests.cc
            src/store/hash.cc
    )
    target_link_libraries(store_tests PRIVATE GTest::gtest_main)
    target_include_directories(store_tests PRIVATE src)

    include(GoogleTest)
    gtest_discover_tests(parser_tests)
    gtest_discover_tests(store_tests)
endif ()
