#include "matching/matching_engine.h"
#include "matching/reference_engine.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <cstdio>

using namespace matching;

// Minimal reproduction: same random sequence, find first divergence
TEST(DifferentialDebug, FindFirstDivergence) {
    constexpr int N = 200;
    constexpr uint32_t SEED = 42;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> type_dist(0, 3);
    std::uniform_int_distribution<int64_t> price_dist(9900, 10100);
    std::uniform_int_distribution<int64_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> cancel_dist(0, 99);

    MatchingEngine optimized(20000, 1000000);
    ReferenceEngine reference;

    std::vector<OrderID> active_orders;

    for (int i = 0; i < N; ++i) {
        OrderID id = static_cast<OrderID>(i + 1);

        if (!active_orders.empty() && cancel_dist(rng) < 10) {
            int idx = std::uniform_int_distribution<int>(
                0, static_cast<int>(active_orders.size()) - 1)(rng);
            OrderID cancel_id = active_orders[idx];
            active_orders[idx] = active_orders.back();
            active_orders.pop_back();

            auto opt = optimized.cancel_order(cancel_id);
            auto ref = reference.cancel_order(cancel_id);
            if (opt.reports.size() != ref.reports.size()) {
                printf("DIVERGENCE at order %d (cancel %lu): reports %zu vs %zu\n",
                       i, cancel_id, opt.reports.size(), ref.reports.size());
                return;
            }
            continue;
        }

        Side side = static_cast<Side>(side_dist(rng));
        OrderType type;
        int type_val = type_dist(rng);
        switch (type_val) {
            case 0: type = OrderType::Limit; break;
            case 1: type = OrderType::Market; break;
            case 2: type = OrderType::IOC; break;
            default: type = OrderType::FOK; break;
        }

        Price price = (type == OrderType::Market) ? 0 : price_dist(rng);
        Quantity qty = qty_dist(rng);

        IncomingOrder order;
        order.id = id;
        order.side = side;
        order.type = type;
        order.price = price;
        order.quantity = qty;
        order.timestamp = id;

        auto opt = optimized.process_order(order);
        auto ref = reference.process_order(order);

        if (opt.trades.size() != ref.trades.size()) {
            printf("DIVERGENCE at order %d (id %lu): trades %zu vs %zu\n",
                   i, id, opt.trades.size(), ref.trades.size());
            printf("  opt reports: %zu, ref reports: %zu\n",
                   opt.reports.size(), ref.reports.size());
            for (size_t j = 0; j < opt.reports.size(); ++j) {
                printf("  opt report[%zu]: id=%lu status=%d filled=%ld remain=%ld\n",
                       j, opt.reports[j].order_id, (int)opt.reports[j].status,
                       opt.reports[j].filled_quantity, opt.reports[j].remaining_quantity);
            }
            for (size_t j = 0; j < ref.reports.size(); ++j) {
                printf("  ref report[%zu]: id=%lu status=%d filled=%ld remain=%ld\n",
                       j, ref.reports[j].order_id, (int)ref.reports[j].status,
                       ref.reports[j].filled_quantity, ref.reports[j].remaining_quantity);
            }
            return;
        }

        if (opt.reports.size() != ref.reports.size()) {
            printf("DIVERGENCE at order %d (id %lu): reports %zu vs %zu\n",
                   i, id, opt.reports.size(), ref.reports.size());
            for (size_t j = 0; j < opt.reports.size(); ++j) {
                printf("  opt report[%zu]: id=%lu status=%d filled=%ld remain=%ld\n",
                       j, opt.reports[j].order_id, (int)opt.reports[j].status,
                       opt.reports[j].filled_quantity, opt.reports[j].remaining_quantity);
            }
            for (size_t j = 0; j < ref.reports.size(); ++j) {
                printf("  ref report[%zu]: id=%lu status=%d filled=%ld remain=%ld\n",
                       j, ref.reports[j].order_id, (int)ref.reports[j].status,
                       ref.reports[j].filled_quantity, ref.reports[j].remaining_quantity);
            }
            return;
        }

        bool opt_ok = optimized.verify_invariants();
        bool ref_ok = reference.verify_invariants();
        if (!opt_ok || !ref_ok) {
            printf("INVARIANT FAILURE at order %d (id %lu): opt=%d ref=%d\n",
                   i, id, opt_ok, ref_ok);
            printf("  best_bid=%ld best_ask=%ld\n",
                   optimized.best_bid(), optimized.best_ask());
            printf("  ref best_bid=%ld ref best_ask=%ld\n",
                   reference.best_bid(), reference.best_ask());
            printf("  opt reports: %zu, ref reports: %zu\n",
                   opt.reports.size(), ref.reports.size());
            for (size_t j = 0; j < opt.reports.size(); ++j) {
                printf("  opt report[%zu]: id=%lu status=%d filled=%ld remain=%ld\n",
                       j, opt.reports[j].order_id, (int)opt.reports[j].status,
                       opt.reports[j].filled_quantity, opt.reports[j].remaining_quantity);
            }
            for (size_t j = 0; j < ref.reports.size(); ++j) {
                printf("  ref report[%zu]: id=%lu status=%d filled=%ld remain=%ld\n",
                       j, ref.reports[j].order_id, (int)ref.reports[j].status,
                       ref.reports[j].filled_quantity, ref.reports[j].remaining_quantity);
            }
            return;
        }

        // Track resting orders
        bool has_fill = false;
        for (const auto& r : opt.reports) {
            if (r.status == OrderStatus::Filled || r.status == OrderStatus::Cancelled) {
                has_fill = true;
            }
        }
        if (type == OrderType::Limit && !has_fill) {
            active_orders.push_back(id);
        }
    }

    printf("No divergence found in %d orders\n", N);
}
