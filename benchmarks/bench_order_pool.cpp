#include "matching/order.h"
#include <benchmark/benchmark.h>

using namespace matching;

// Benchmark: allocate + deallocate cycle
static void BM_OrderPool_AllocDealloc(benchmark::State& state) {
    OrderPool pool(1000000);

    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            Order* o = pool.allocate();
            o->id = i;
            pool.deallocate(o);
        }
    }
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_OrderPool_AllocDealloc);

// Benchmark: allocate until full, then deallocate all
static void BM_OrderPool_FillAndDrain(benchmark::State& state) {
    constexpr int N = 100000;
    OrderPool pool(N);

    for (auto _ : state) {
        // Fill
        std::vector<Order*> orders;
        orders.reserve(N);
        for (int i = 0; i < N; ++i) {
            Order* o = pool.allocate();
            o->id = i;
            orders.push_back(o);
        }
        // Drain
        for (Order* o : orders) {
            pool.deallocate(o);
        }
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_OrderPool_FillAndDrain);

// Benchmark: PriceLevelList insert + remove
static void BM_PriceLevelList_Operations(benchmark::State& state) {
    OrderPool pool(1000000);
    PriceLevelList list;

    for (auto _ : state) {
        // Insert 100 orders
        for (int i = 0; i < 100; ++i) {
            Order* o = pool.allocate();
            o->id = i;
            list.push_back(o);
        }
        // Remove all from front
        while (!list.empty()) {
            Order* o = list.front();
            list.remove(o);
            pool.deallocate(o);
        }
    }
    state.SetItemsProcessed(state.iterations() * 200); // 100 inserts + 100 removes
}
BENCHMARK(BM_PriceLevelList_Operations);
