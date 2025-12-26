cmake_minimum_required(VERSION 3.21)
project(my-redis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# Server files
set(RESPVALUE_SOURCES
        src/server/respvalue/RespValue.cpp
        src/server/respvalue/RespValueQueue.cpp
        src/server/respvalue/RespValues.cpp
)

set(REQUEST_SOURCES
        src/server/requests/GetRequest.cpp
        src/server/requests/SetRequest.cpp
        src/server/requests/DelRequest.cpp
        src/server/requests/OneArgRequest.cpp
        src/server/requests/PublishRequest.cpp
        src/server/requests/SubscribeRequest.cpp
        src/server/requests/UnsubscribeRequest.cpp
        src/server/requests/VarArgRequest.cpp
)

set(STORE_SOURCES
        src/server/store/Hash.cpp
)

set(LOGGING_SOURCES
        src/server/logger/FileLogger.cpp
)

set(PUBSUB_SOURCES
        src/server/pubsub/PubSubChannels.cpp
        src/server/pubsub/PubSubResponse.cpp
)

set(HANDLER_SOURCES
        src/server/handler/GetRequestHandler.cpp
        src/server/handler/HandlerDispatcher.cpp
        src/server/handler/SetRequestHandler.cpp
        src/server/handler/DelRequestHandler.cpp
        src/server/handler/SubscribeRequestHandler.cpp
        src/server/handler/PublishRequestHandler.cpp
        src/server/handler/UnsubscribeRequestHandler.cpp
        src/server/handler/UnknownRequestHandler.cpp
)

set(TOP_SOURCES
        src/server/ServerApp.cpp
        src/server/RequestExecutor.cpp
        src/server/HandlerHelpers.cpp
)

# Consolidated application sources
set(APP_SOURCES
        ${RESPVALUE_SOURCES}
        ${REQUEST_SOURCES}
        ${STORE_SOURCES}
        ${LOGGING_SOURCES}
        ${PUBSUB_SOURCES}
        ${HANDLER_SOURCES}
        ${TOP_SOURCES}
)

# Server executable
add_executable(my_redis_server src/server/main.cpp ${APP_SOURCES})
target_include_directories(my_redis_server PRIVATE src/server)

# Client executable
add_executable(my_redis_client src/client/main.cpp)
target_include_directories(my_redis_client PRIVATE src/client)

# --- Testing ---
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
            tests/ParserTests.cpp
            src/server/respvalue/RespValue.cpp
    )
    add_executable(map_tests
            tests/MapTests.cpp
            ${STORE_SOURCES}
    )

    # Link the test executable against Google Test
    target_link_libraries(parser_tests PRIVATE GTest::gtest_main)
    target_link_libraries(map_tests PRIVATE GTest::gtest_main)

    # Include directories for the test executable
    target_include_directories(parser_tests PRIVATE src/server)
    target_include_directories(map_tests PRIVATE src/server)

    include(GoogleTest)
    gtest_discover_tests(parser_tests)
    gtest_discover_tests(map_tests)
endif ()
