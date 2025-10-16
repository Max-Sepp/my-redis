cmake_minimum_required(VERSION 3.21)
project(my-redis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# It's better to define source files in variables
set(APP_SOURCES
        main.cpp
        src/Parser/Parser.cpp
)

set(LIB_SOURCES
        src/Parser/Parser.cpp
)

set(LIB_HEADERS
        src/Parser/Parser.h
)

# Main executable
add_executable(my_redis ${APP_SOURCES})
target_include_directories(my_redis PRIVATE src)

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
    add_executable(redis_test
            tests/ParserTests.cpp
            ${LIB_SOURCES}
    )

    # Link the test executable against Google Test
    target_link_libraries(redis_test PRIVATE GTest::gtest_main)

    # Include directories for the test executable
    target_include_directories(redis_test PRIVATE src)

    include(GoogleTest)
    gtest_discover_tests(redis_test)
endif ()