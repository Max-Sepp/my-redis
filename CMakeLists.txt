cmake_minimum_required(VERSION 3.21)
project(my-redis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# It's better to define source files in variables
set(APP_SOURCES
        src/main.cpp
        src/parser/Parser.cpp
        src/store/LinkedListHashmap.h
)

set(LIB_SOURCES
        src/parser/Parser.cpp
)

set(LIB_HEADERS
        src/parser/Parser.h
        src/store/LinearProbingHashmap.h
        src/store/StandardMap.h
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
    add_executable(parser_tests
            tests/ParserTests.cpp
            ${LIB_SOURCES}
    )
    add_executable(map_tests
            tests/MapTests.cpp
            ${LIB_SOURCES}
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