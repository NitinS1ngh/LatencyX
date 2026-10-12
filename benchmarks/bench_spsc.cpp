#include "matching/spsc_queue.h"
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>

using namespace matching;

// SPSC push throughput (single-threaded, no contention)
static void BM_SPSC_Push(benchmark::State& state) {
    SPSCQueue<uint64_t, 65536> q;

    for (auto _ : state) {
        for (uint64_t i = 0; i < 1000; ++i) {
            q.try_push(i);
        }
    }
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_SPSC_Push);

// SPSC pop throughput (single-threaded, no contention)
static void BM_SPSC_Pop(benchmark::State& state) {
    SPSCQueue<uint64_t, 65536> q;

    for (auto _ : state) {
        // Fill
        for (uint64_t i = 0; i < 1000; ++i) {
            q.try_push(i);
        }
        // Drain
        uint64_t val;
        for (int i = 0; i < 1000; ++i) {
            q.try_pop(val);
        }
    }
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_SPSC_Pop);

// SPSC concurrent throughput (producer + consumer on separate cores)
static void BM_SPSC_Concurrent(benchmark::State& state) {
    constexpr int BATCH = 10000;
    SPSCQueue<uint64_t, 65536> q;

    for (auto _ : state) {
        std::atomic<bool> done{false};
        std::atomic<uint64_t> sum{0};

        std::thread producer([&]() {
            for (uint64_t i = 0; i < BATCH; ++i) {
                while (!q.try_push(i)) {}
            }
        });

        std::thread consumer([&]() {
            uint64_t local_sum = 0;
            uint64_t val;
            for (int i = 0; i < BATCH; ++i) {
                while (!q.try_pop(val)) {}
                local_sum += val;
            }
            sum.store(local_sum, std::memory_order_relaxed);
        });

        producer.join();
        consumer.join();
    }
    state.SetItemsProcessed(state.iterations() * BATCH);
}
BENCHMARK(BM_SPSC_Concurrent);
