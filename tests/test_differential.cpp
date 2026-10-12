#include "matching/matching_engine.h"
#include "matching/reference_engine.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace matching;

// Helper to compare trade events between the two engines.
// We compare on logical fields, not timestamps (which may differ).
static void compare_trades(const std::vector<Trade>& a, const std::vector<Trade>& b) {
    ASSERT_EQ(a.size(), b.size())
        << "Trade count mismatch: " << a.size() << " vs " << b.size();
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].buy_order_id, b[i].buy_order_id)
            << "Trade " << i << ": buy_order_id mismatch";
        EXPECT_EQ(a[i].sell_order_id, b[i].sell_order_id)
            << "Trade " << i << ": sell_order_id mismatch";
        EXPECT_EQ(a[i].price, b[i].price)
            << "Trade " << i << ": price mismatch";
        EXPECT_EQ(a[i].quantity, b[i].quantity)
            << "Trade " << i << ": quantity mismatch";
    }
}

static void compare_reports(const std::vector<ExecutionReport>& a,
                            const std::vector<ExecutionReport>& b) {
    ASSERT_EQ(a.size(), b.size())
        << "Report count mismatch: " << a.size() << " vs " << b.size();
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].order_id, b[i].order_id)
            << "Report " << i << ": order_id mismatch";
        EXPECT_EQ(a[i].status, b[i].status)
            << "Report " << i << ": status mismatch for order " << a[i].order_id;
        EXPECT_EQ(a[i].filled_quantity, b[i].filled_quantity)
            << "Report " << i << ": filled_quantity mismatch for order " << a[i].order_id;
        EXPECT_EQ(a[i].remaining_quantity, b[i].remaining_quantity)
            << "Report " << i << ": remaining_quantity mismatch for order " << a[i].order_id;
    }
}

// Deterministic test: fixed sequence of orders
TEST(DifferentialTest, FixedSequence) {
    MatchingEngine optimized(20000, 100000);
    ReferenceEngine reference;

    std::vector<IncomingOrder> orders = {
        // Rest some sell orders
        {101, Side::Sell, OrderType::Limit, 105, 50, 1},
        {102, Side::Sell, OrderType::Limit, 106, 30, 2},
        {103, Side::Sell, OrderType::Limit, 105, 20, 3},  // same price, after 101

        // Rest some buy orders
        {201, Side::Buy,  OrderType::Limit, 100, 40, 4},
        {202, Side::Buy,  OrderType::Limit, 101, 60, 5},

        // Buy that crosses multiple levels
        {301, Side::Buy,  OrderType::Limit, 107, 100, 6},

        // Sell that crosses
        {401, Side::Sell, OrderType::Limit, 99, 50, 7},

        // Market buy
        {501, Side::Buy,  OrderType::Market, 0, 30, 8},

        // More resting orders
        {601, Side::Sell, OrderType::Limit, 103, 100, 9},
        {602, Side::Sell, OrderType::Limit, 103, 50, 10},

        // Buy crosses all of 103
        {701, Side::Buy,  OrderType::Limit, 103, 200, 11},
    };

    for (const auto& order : orders) {
        auto opt_event = optimized.process_order(order);
        auto ref_event = reference.process_order(order);

        compare_trades(opt_event.trades, ref_event.trades);
        compare_reports(opt_event.reports, ref_event.reports);

        EXPECT_TRUE(optimized.verify_invariants())
            << "Optimized engine invariant failed after order " << order.id;
        EXPECT_TRUE(reference.verify_invariants())
            << "Reference engine invariant failed after order " << order.id;
    }
}

// Randomized test with a fixed seed for reproducibility
TEST(DifferentialTest, RandomizedFuzz) {
    constexpr int NUM_ORDERS = 5000;
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

    for (int i = 0; i < NUM_ORDERS; ++i) {
        OrderID id = static_cast<OrderID>(i + 1);

        // 10% chance of cancel if we have active orders
        if (!active_orders.empty() && cancel_dist(rng) < 10) {
            int idx = std::uniform_int_distribution<int>(
                0, static_cast<int>(active_orders.size()) - 1)(rng);
            OrderID cancel_id = active_orders[idx];
            active_orders[idx] = active_orders.back();
            active_orders.pop_back();

            auto opt_event = optimized.cancel_order(cancel_id);
            auto ref_event = reference.cancel_order(cancel_id);

            compare_reports(opt_event.reports, ref_event.reports);
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

        auto opt_event = optimized.process_order(order);
        auto ref_event = reference.process_order(order);

        compare_trades(opt_event.trades, ref_event.trades);
        compare_reports(opt_event.reports, ref_event.reports);

        EXPECT_TRUE(optimized.verify_invariants())
            << "Invariant failed after order " << id;
        EXPECT_TRUE(reference.verify_invariants())
            << "Invariant failed after order " << id;

        // Track orders that might still be resting
        bool has_fill = false;
        for (const auto& r : opt_event.reports) {
            if (r.status == OrderStatus::Filled || r.status == OrderStatus::Cancelled) {
                has_fill = true;
            }
        }
        // If the order might be resting (limit, not fully consumed), track it
        if (type == OrderType::Limit && !has_fill) {
            active_orders.push_back(id);
        }
    }
}

// Stress test: rapid cancel/amend patterns
TEST(DifferentialTest, CancelStorm) {
    constexpr int N = 2000;
    MatchingEngine optimized(20000, 100000);
    ReferenceEngine reference;

    // Place many orders
    for (int i = 0; i < N; ++i) {
        IncomingOrder o;
        o.id = static_cast<OrderID>(i + 1);
        o.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        o.type = OrderType::Limit;
        o.price = (o.side == Side::Buy) ? 10000 : 10005;
        o.quantity = 10;
        o.timestamp = o.id;

        auto opt = optimized.process_order(o);
        auto ref = reference.process_order(o);
        compare_trades(opt.trades, ref.trades);
        compare_reports(opt.reports, ref.reports);
    }

    // Cancel every other order
    for (int i = 0; i < N; i += 2) {
        OrderID cancel_id = static_cast<OrderID>(i + 1);
        auto opt = optimized.cancel_order(cancel_id);
        auto ref = reference.cancel_order(cancel_id);
        compare_reports(opt.reports, ref.reports);
    }

    EXPECT_TRUE(optimized.verify_invariants());
    EXPECT_TRUE(reference.verify_invariants());
}
