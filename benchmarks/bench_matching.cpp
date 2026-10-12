#include "matching/matching_engine.h"
#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <optional>

using namespace matching;

// Generate a realistic order mix for benchmarking
static std::vector<IncomingOrder> generate_orders(int n, uint32_t seed = 12345) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int64_t> price_dist(9950, 10050);
    std::uniform_int_distribution<int64_t> qty_dist(1, 100);
    std::uniform_real_distribution<double> market_prob(0.0, 1.0);

    std::vector<IncomingOrder> orders;
    orders.reserve(n);

    for (int i = 0; i < n; ++i) {
        IncomingOrder o;
        o.id = static_cast<OrderID>(i + 1);
        o.side = static_cast<Side>(side_dist(rng));

        bool is_market = market_prob(rng) < 0.2;
        o.type = is_market ? OrderType::Market : OrderType::Limit;
        o.price = is_market ? 0 : price_dist(rng);
        o.quantity = qty_dist(rng);
        o.timestamp = o.id;

        orders.push_back(o);
    }
    return orders;
}

// Benchmark: process a batch of mixed orders (no cancellation).
//
// Engine construction and destruction are excluded from measured time.
// Each iteration starts with an identical empty book so that matching
// behaviour is deterministic and pool state cannot carry over between
// iterations.  std::optional<MatchingEngine> lets us destroy the engine
// from the previous iteration and construct a fresh one, both under
// PauseTiming(), before the timed order-processing loop begins.
static void BM_MatchingEngine_Throughput(benchmark::State& state) {
    const int batch_size = static_cast<int>(state.range(0));
    auto orders = generate_orders(batch_size);

    // Declared outside the loop so the destructor of the previous
    // iteration's engine runs under PauseTiming() at the start of the
    // next iteration (via reset+emplace), not inside the timed region.
    std::optional<MatchingEngine> engine;

    for (auto _ : state) {
        // --- untimed setup ---
        state.PauseTiming();
        engine.reset();                        // destroy previous iteration's engine
        engine.emplace(20000, 1000000);        // construct fresh engine (allocates pool + ladders)
        state.ResumeTiming();

        // --- timed work: only order processing ---
        for (const auto& order : orders) {
            engine->process_order(order);
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_MatchingEngine_Throughput)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000);

// Benchmark: single order processing (latency measurement)
static void BM_MatchingEngine_SingleOrder(benchmark::State& state) {
    MatchingEngine engine(20000, 1000000);

    // Warm up the book with some resting orders
    for (int i = 0; i < 1000; ++i) {
        IncomingOrder o;
        o.id = static_cast<OrderID>(i + 1);
        o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        o.type = OrderType::Limit;
        o.price = (o.side == Side::Buy) ? 10000 : 10005;
        o.quantity = 100;
        o.timestamp = o.id;
        engine.process_order(o);
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(9998, 10007);
    std::uniform_int_distribution<int64_t> qty_dist(1, 10);

    for (auto _ : state) {
        IncomingOrder o;
        o.id = 1000000 + state.iterations();
        o.side = Side::Buy;
        o.type = OrderType::Limit;
        o.price = price_dist(rng);
        o.quantity = qty_dist(rng);
        o.timestamp = o.id;

        auto event = engine.process_order(o);
        benchmark::DoNotOptimize(&event);
    }
}
BENCHMARK(BM_MatchingEngine_SingleOrder);

// Benchmark: cancel orders
static void BM_MatchingEngine_Cancel(benchmark::State& state) {
    const int n = 10000;
    MatchingEngine engine(20000, 1000000);

    // Pre-populate
    for (int i = 0; i < n; ++i) {
        IncomingOrder o;
        o.id = static_cast<OrderID>(i + 1);
        o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        o.type = OrderType::Limit;
        o.price = (o.side == Side::Buy) ? 10000 : 10005;
        o.quantity = 10;
        o.timestamp = o.id;
        engine.process_order(o);
    }

    for (auto _ : state) {
        // Cancel half of them
        for (int i = 0; i < n; i += 2) {
            engine.cancel_order(static_cast<OrderID>(i + 1));
        }
    }
    state.SetItemsProcessed(state.iterations() * (n / 2));
}
BENCHMARK(BM_MatchingEngine_Cancel);
