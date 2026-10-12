#include "matching/matching_engine.h"
#include "matching/reference_engine.h"
#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstdio>

using namespace matching;

// Logarithmic latency histogram for nanosecond-precision percentiles.
class LatencyHistogram {
public:
    static constexpr int NUM_BUCKETS = 4096;
    static constexpr double MIN_VALUE_NS = 1.0;  // 1 nanosecond
    static constexpr double MAX_VALUE_NS = 1000000.0;  // 1 millisecond

    LatencyHistogram() : buckets_(NUM_BUCKETS, 0), count_(0), sum_(0),
                          min_(MAX_VALUE_NS), max_(0) {}

    void record(double value_ns) {
        int bucket = bucket_for(value_ns);
        if (bucket >= 0 && bucket < NUM_BUCKETS) {
            buckets_[bucket]++;
        }
        count_++;
        sum_ += value_ns;
        if (value_ns < min_) min_ = value_ns;
        if (value_ns > max_) max_ = value_ns;
    }

    double percentile(double p) const {
        if (count_ == 0) return 0;
        uint64_t target = static_cast<uint64_t>(std::ceil(p / 100.0 * count_));
        uint64_t cumulative = 0;
        for (int i = 0; i < NUM_BUCKETS; ++i) {
            cumulative += buckets_[i];
            if (cumulative >= target) {
                return bucket_to_value(i);
            }
        }
        return max_;
    }

    double mean() const { return count_ > 0 ? sum_ / count_ : 0; }
    double min() const { return min_; }
    double max() const { return max_; }
    uint64_t count() const { return count_; }

    void report() const {
        printf("  Count:   %lu\n", count_);
        printf("  Mean:    %.1f ns\n", mean());
        printf("  Min:     %.1f ns\n", min_);
        printf("  Max:     %.1f ns\n", max_);
        printf("  p50:     %.1f ns\n", percentile(50));
        printf("  p90:     %.1f ns\n", percentile(90));
        printf("  p99:     %.1f ns\n", percentile(99));
        printf("  p99.9:   %.1f ns\n", percentile(99.9));
        printf("  p99.99:  %.1f ns\n", percentile(99.99));
    }

private:
    int bucket_for(double value_ns) const {
        if (value_ns < MIN_VALUE_NS) return 0;
        if (value_ns >= MAX_VALUE_NS) return NUM_BUCKETS - 1;
        // Logarithmic bucketing: bucket = log2(value / MIN) * (NUM / log2(MAX / MIN))
        double ratio = value_ns / MIN_VALUE_NS;
        double log_val = std::log2(ratio);
        double max_log = std::log2(MAX_VALUE_NS / MIN_VALUE_NS);
        return static_cast<int>(log_val / max_log * (NUM_BUCKETS - 1));
    }

    double bucket_to_value(int bucket) const {
        double max_log = std::log2(MAX_VALUE_NS / MIN_VALUE_NS);
        double log_val = static_cast<double>(bucket) / (NUM_BUCKETS - 1) * max_log;
        return MIN_VALUE_NS * std::pow(2.0, log_val);
    }

    std::vector<uint64_t> buckets_;
    uint64_t count_;
    double sum_;
    double min_;
    double max_;
};

// Benchmark: measure per-order latency with high-resolution timer
static void BM_MatchingEngine_LatencyHistogram(benchmark::State& state) {
    const int warmup = 1000;
    const int measurement = 10000;

    // Warm up
    {
        MatchingEngine engine(20000, 1000000);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int64_t> price_dist(9998, 10007);
        std::uniform_int_distribution<int64_t> qty_dist(1, 10);

        for (int i = 0; i < warmup; ++i) {
            IncomingOrder o;
            o.id = i + 1;
            o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            o.type = (i % 5 == 0) ? OrderType::Market : OrderType::Limit;
            o.price = o.type == OrderType::Market ? 0 : price_dist(rng);
            o.quantity = qty_dist(rng);
            o.timestamp = o.id;
            engine.process_order(o);
        }
    }

    for (auto _ : state) {
        MatchingEngine engine(20000, 1000000);
        LatencyHistogram hist;

        std::mt19937 rng(42);
        std::uniform_int_distribution<int64_t> price_dist(9998, 10007);
        std::uniform_int_distribution<int64_t> qty_dist(1, 10);

        // Pre-populate with some resting orders
        for (int i = 0; i < 100; ++i) {
            IncomingOrder o;
            o.id = i + 1;
            o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            o.type = OrderType::Limit;
            o.price = (o.side == Side::Buy) ? 10000 : 10005;
            o.quantity = 100;
            o.timestamp = o.id;
            engine.process_order(o);
        }

        // Measure latency of each order
        for (int i = 0; i < measurement; ++i) {
            IncomingOrder o;
            o.id = 10000 + i;
            o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            o.type = (i % 10 == 0) ? OrderType::Market : OrderType::Limit;
            o.price = o.type == OrderType::Market ? 0 : price_dist(rng);
            o.quantity = qty_dist(rng);
            o.timestamp = o.id;

            auto start = std::chrono::high_resolution_clock::now();
            engine.process_order(o);
            auto end = std::chrono::high_resolution_clock::now();

            double ns = std::chrono::duration<double, std::nano>(end - start).count();
            hist.record(ns);
        }

        // Print results (only on first iteration to avoid spam)
        if (state.iterations() == 0) {
            printf("\n=== Latency Histogram (per-order) ===\n");
            hist.report();
            printf("\n");
        }
    }
}
BENCHMARK(BM_MatchingEngine_LatencyHistogram);

// Benchmark: compare naive vs optimized throughput
static void BM_NaiveVsOptimized(benchmark::State& state) {
    const int n = 1000;
    std::vector<IncomingOrder> orders;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(9998, 10007);
    std::uniform_int_distribution<int64_t> qty_dist(1, 50);

    for (int i = 0; i < n; ++i) {
        IncomingOrder o;
        o.id = i + 1;
        o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        o.type = OrderType::Limit;
        o.price = price_dist(rng);
        o.quantity = qty_dist(rng);
        o.timestamp = o.id;
        orders.push_back(o);
    }

    for (auto _ : state) {
        ReferenceEngine engine;
        for (const auto& order : orders) {
            engine.process_order(order);
        }
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_NaiveVsOptimized);
