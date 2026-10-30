cmake_minimum_required(VERSION 3.21)

option(BUILD_TESTS "Build the tests" OFF)

# Automatically enable tests for Debug builds unless the user explicitly disabled them.
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT BUILD_TESTS)
    set(BUILD_TESTS ON CACHE BOOL "Build the tests" FORCE)
endif ()

# vcpkg installs dependencies during project(), so the feature must be set first.
if (BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif ()

project(my-redis)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(cxxopts CONFIG REQUIRED)

# --- Reusable building blocks ------------------------------------------------

set(RESP_VALUE_SOURCES
        src/resp_value/resp_value.cc
        src/resp_value/resp_value_queue.cc
        src/resp_value/resp_values.cc
        src/resp_value/resp_writer.cc
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
target_link_libraries(my_redis_client PRIVATE cxxopts::cxxopts)

# --- Server executable -------------------------------------------------------
# A single main thread executes commands; a pool of IO threads handle sockets
# and RESP parsing.

set(SERVER_SOURCES
        ${RESP_VALUE_SOURCES}
        src/concurrent/event_fd.cc
        src/server/connection.cc
        src/server/io_thread.cc
        src/maintenance/maintenance_scheduler.cc
        src/server/server.cc
        src/snapshot/snapshotter.cc
        src/store/serialise.cc
        src/store/store.cc
        src/data_structures/map/hash.cc
)

add_executable(my_redis_server src/server/main.cc ${SERVER_SOURCES})
target_include_directories(my_redis_server PRIVATE src)
target_link_libraries(my_redis_server PRIVATE cxxopts::cxxopts)

# --- Testing -----------------------------------------------------------------
# Only include tests if the TESTS option is ON (default: OFF)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "BUILD_TESTS = ${BUILD_TESTS}")

if (BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)

    enable_testing()

    # Add the test executable with its own sources plus the library sources
    add_executable(parser_tests
            tests/parser_tests.cc
            src/resp_value/resp_value.cc
            src/resp_value/resp_writer.cc
    )

    # Link the test executable against Google Test
    target_link_libraries(parser_tests PRIVATE GTest::gtest_main)

    # Include directories for the test executable
    target_include_directories(parser_tests PRIVATE src)

    # Store tests: the non-concurrent maps are header-only and depend on the
    # data_structures/map/hash.cc helper.
    add_executable(store_tests
            tests/store_tests.cc
            src/data_structures/map/hash.cc
    )
    target_link_libraries(store_tests PRIVATE GTest::gtest_main)
    target_include_directories(store_tests PRIVATE src)

    add_executable(store_expiry_tests
            tests/store_expiry_tests.cc
            src/store/serialise.cc
            src/store/store.cc
            src/data_structures/map/hash.cc
    )
    target_link_libraries(store_expiry_tests PRIVATE GTest::gtest_main)
    target_include_directories(store_expiry_tests PRIVATE src)

    add_executable(maintenance_tests
            tests/maintenance_tests.cc
            src/maintenance/maintenance_scheduler.cc
            src/store/serialise.cc
            src/store/store.cc
            src/data_structures/map/hash.cc
    )
    target_link_libraries(maintenance_tests PRIVATE GTest::gtest_main)
    target_include_directories(maintenance_tests PRIVATE src)

    add_executable(batched_ring_buffer_tests
            tests/batched_ring_buffer_tests.cc
    )
    target_link_libraries(batched_ring_buffer_tests PRIVATE GTest::gtest_main)
    target_include_directories(batched_ring_buffer_tests PRIVATE src)

    include(GoogleTest)
    gtest_discover_tests(parser_tests)
    gtest_discover_tests(store_tests)
    gtest_discover_tests(store_expiry_tests)
    gtest_discover_tests(maintenance_tests)
    gtest_discover_tests(batched_ring_buffer_tests)
endif ()
