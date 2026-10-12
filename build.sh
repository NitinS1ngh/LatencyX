#!/bin/bash
set -e

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++20 -O2 -Wall -Wextra -I include"
BUILD_DIR="build"

mkdir -p "$BUILD_DIR"

echo "=== Building matching_core library ==="
$CXX $CXXFLAGS -c src/order_pool.cpp -o "$BUILD_DIR/order_pool.o"
$CXX $CXXFLAGS -c src/price_ladder.cpp -o "$BUILD_DIR/price_ladder.o"
$CXX $CXXFLAGS -c src/matching_engine.cpp -o "$BUILD_DIR/matching_engine.o"
$CXX $CXXFLAGS -c src/reference_engine.cpp -o "$BUILD_DIR/reference_engine.o"
$CXX $CXXFLAGS -c src/spsc_queue.cpp -o "$BUILD_DIR/spsc_queue.o"
$CXX $CXXFLAGS -c src/order_gateway.cpp -o "$BUILD_DIR/order_gateway.o"
$CXX $CXXFLAGS -c src/market_data.cpp -o "$BUILD_DIR/market_data.o"

# Create static library
ar rcs "$BUILD_DIR/libmatching_core.a" \
    "$BUILD_DIR/order_pool.o" \
    "$BUILD_DIR/price_ladder.o" \
    "$BUILD_DIR/matching_engine.o" \
    "$BUILD_DIR/reference_engine.o" \
    "$BUILD_DIR/spsc_queue.o" \
    "$BUILD_DIR/order_gateway.o" \
    "$BUILD_DIR/market_data.o"

echo "Core library: $BUILD_DIR/libmatching_core.a"

# Download and build Google Test if needed
if [ ! -f "$BUILD_DIR/googletest/build/lib/libgtest.a" ]; then
    echo "=== Setting up Google Test ==="
    cd "$BUILD_DIR"
    if [ ! -d "googletest" ]; then
        git clone --depth 1 https://github.com/google/googletest.git
    fi
    cd googletest
    mkdir -p build && cd build
    cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release 2>/dev/null
    make -j$(nproc)
    cd ../..
    cd ..
fi

GTEST_INC="-I $BUILD_DIR/googletest/googletest/include -I $BUILD_DIR/googletest/googlemock/include"
GTEST_LIB="$BUILD_DIR/googletest/build/lib/libgtest.a $BUILD_DIR/googletest/build/lib/libgtest_main.a"

echo "=== Building tests ==="
TEST_SRCS=(
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

TEST_OBJS=()
for f in "${TEST_SRCS[@]}"; do
    name=$(basename "$f" .cpp)
    echo "  Compiling $name..."
    $CXX $CXXFLAGS $GTEST_INC -c "$f" -o "$BUILD_DIR/${name}.o"
    TEST_OBJS+=("$BUILD_DIR/${name}.o")
done

echo "  Linking tests..."
$CXX "${TEST_OBJS[@]}" "$BUILD_DIR/libmatching_core.a" $GTEST_LIB -lpthread -o "$BUILD_DIR/matching_tests"

# Download and build Google Benchmark if needed
if [ ! -f "$BUILD_DIR/googlebenchmark/build/src/libbenchmark.a" ]; then
    echo "=== Setting up Google Benchmark ==="
    cd "$BUILD_DIR"
    if [ ! -d "googlebenchmark" ]; then
        git clone --depth 1 https://github.com/google/benchmark.git googlebenchmark
    fi
    cd googlebenchmark
    mkdir -p build && cd build
    cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_TESTING=OFF 2>/dev/null
    make -j$(nproc)
    cd ../..
    cd ..
fi

if [ -f "$BUILD_DIR/googlebenchmark/build/src/libbenchmark.a" ]; then
    echo "=== Building benchmarks ==="
    BENCH_INC="-I $BUILD_DIR/googlebenchmark/include"
    BENCH_LIB="$BUILD_DIR/googlebenchmark/build/src/libbenchmark.a $BUILD_DIR/googlebenchmark/build/src/libbenchmark_main.a"
    BENCH_SRCS=(
        benchmarks/bench_matching.cpp
        benchmarks/bench_spsc.cpp
        benchmarks/bench_order_pool.cpp
        benchmarks/bench_latency_histogram.cpp
    )
    BENCH_OBJS=()
    for f in "${BENCH_SRCS[@]}"; do
        name=$(basename "$f" .cpp)
        echo "  Compiling $name..."
        $CXX $CXXFLAGS -O3 -march=native $BENCH_INC -c "$f" -o "$BUILD_DIR/${name}.o"
        BENCH_OBJS+=("$BUILD_DIR/${name}.o")
    done
    echo "  Linking benchmarks..."
    $CXX "${BENCH_OBJS[@]}" "$BUILD_DIR/libmatching_core.a" $BENCH_LIB -lpthread -o "$BUILD_DIR/matching_bench"
fi

echo ""
echo "=== Build complete ==="
echo "Run tests: $BUILD_DIR/matching_tests"
if [ -f "$BUILD_DIR/matching_bench" ]; then
    echo "Run benchmarks: $BUILD_DIR/matching_bench"
fi
echo "Object files in $BUILD_DIR/:"
ls -la "$BUILD_DIR/"*.o "$BUILD_DIR/"*.a 2>/dev/null
