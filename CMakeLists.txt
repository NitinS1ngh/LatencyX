cmake_minimum_required(VERSION 3.20)
project(MatchingEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core library
add_library(matching_core STATIC
    src/order_pool.cpp
    src/price_ladder.cpp
    src/matching_engine.cpp
    src/reference_engine.cpp
    src/spsc_queue.cpp
    src/order_gateway.cpp
    src/market_data.cpp
)
target_include_directories(matching_core PUBLIC include)

# Release flags
target_compile_options(matching_core PUBLIC
    -Wall -Wextra -Wpedantic
    -O3
)

# Sanitizer targets
add_library(matching_core_asan STATIC EXCLUDE_FROM_ALL
    src/order_pool.cpp
    src/price_ladder.cpp
    src/matching_engine.cpp
    src/reference_engine.cpp
    src/spsc_queue.cpp
    src/order_gateway.cpp
    src/market_data.cpp
)
target_include_directories(matching_core_asan PUBLIC include)
target_compile_options(matching_core_asan PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
target_link_options(matching_core_asan PUBLIC -fsanitize=address,undefined)

add_library(matching_core_tsan STATIC EXCLUDE_FROM_ALL
    src/order_pool.cpp
    src/price_ladder.cpp
    src/matching_engine.cpp
    src/reference_engine.cpp
    src/spsc_queue.cpp
    src/order_gateway.cpp
    src/market_data.cpp
)
target_include_directories(matching_core_tsan PUBLIC include)
target_compile_options(matching_core_tsan PUBLIC -fsanitize=thread -g -O1)
target_link_options(matching_core_tsan PUBLIC -fsanitize=thread)

# Tests
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(matching_tests
    tests/test_order_pool.cpp
    tests/test_bitset.cpp
    tests/test_spsc_queue.cpp
    tests/test_matching_engine.cpp
    tests/test_reference_engine.cpp
    tests/test_differential.cpp
    tests/test_differential_debug.cpp
    tests/test_order_gateway.cpp
    tests/test_market_data.cpp
)
target_link_libraries(matching_tests PRIVATE matching_core GTest::gtest_main)

# ASAN test target
add_executable(matching_tests_asan EXCLUDE_FROM_ALL
    tests/test_order_pool.cpp
    tests/test_bitset.cpp
    tests/test_spsc_queue.cpp
    tests/test_matching_engine.cpp
    tests/test_reference_engine.cpp
    tests/test_differential.cpp
    tests/test_differential_debug.cpp
    tests/test_order_gateway.cpp
    tests/test_market_data.cpp
)
target_link_libraries(matching_tests_asan PRIVATE matching_core_asan GTest::gtest_main)
target_compile_options(matching_tests_asan PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
target_link_options(matching_tests_asan PRIVATE -fsanitize=address,undefined)

# TSAN test target
add_executable(matching_tests_tsan EXCLUDE_FROM_ALL
    tests/test_order_pool.cpp
    tests/test_bitset.cpp
    tests/test_spsc_queue.cpp
    tests/test_matching_engine.cpp
    tests/test_reference_engine.cpp
    tests/test_differential.cpp
    tests/test_differential_debug.cpp
    tests/test_order_gateway.cpp
    tests/test_market_data.cpp
)
target_link_libraries(matching_tests_tsan PRIVATE matching_core_tsan GTest::gtest_main)
target_compile_options(matching_tests_tsan PRIVATE -fsanitize=thread -g -O1)
target_link_options(matching_tests_tsan PRIVATE -fsanitize=thread)

include(GoogleTest)
gtest_discover_tests(matching_tests)

# Benchmarks
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(matching_bench
    benchmarks/bench_matching.cpp
    benchmarks/bench_spsc.cpp
    benchmarks/bench_order_pool.cpp
    benchmarks/bench_latency_histogram.cpp
)
target_link_libraries(matching_bench PRIVATE matching_core benchmark::benchmark benchmark::benchmark_main)
target_compile_options(matching_bench PRIVATE -O3 -march=native -flto)
target_link_options(matching_bench PRIVATE -flto)
