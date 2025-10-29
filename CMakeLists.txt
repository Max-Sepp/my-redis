cmake_minimum_required(VERSION 3.21)
project(my-redis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# It's better to define source files in variables
set(APP_SOURCES
        src/respvalue/RespValue.cpp
        src/respvalue/RecvBufferIterator.cpp
        src/requests/GetRequest.cpp
        src/Handler.cpp
        src/requests/SetRequest.cpp
        src/store/Hash.cpp
        src/respvalue/RespValues.cpp
        src/logging/FileLogger.cpp
)

set(LIB_HEADERS
        src/respvalue/RespValue.h
        src/respvalue/RecvBufferIterator.h
        src/store/LinearProbingHashmap.h
        src/store/StandardMap.h
)

# Main executable
add_executable(my_redis_server src/server.cpp ${APP_SOURCES})
target_include_directories(my_redis_server PRIVATE src)

# --- Testing ---
# Only include tests if the TESTS option is ON (default)
option(BUILD_TESTS "Build the tests" ON)

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
            ${APP_SOURCES}
    )
    add_executable(map_tests
            tests/MapTests.cpp
            ${APP_SOURCES}
    )

    # Link the test executable against Google Test
    target_link_libraries(parser_tests PRIVATE GTest::gtest_main)
    target_link_libraries(map_tests PRIVATE GTest::gtest_main)

    # Include directories for the test executable
    target_include_directories(parser_tests PRIVATE src)
    target_include_directories(map_tests PRIVATE src)

    include(GoogleTest)
    gtest_discover_tests(parser_tests)
    gtest_discover_tests(map_tests)
endif ()