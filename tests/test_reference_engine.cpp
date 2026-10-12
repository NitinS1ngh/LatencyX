#include "matching/reference_engine.h"
#include <gtest/gtest.h>

using namespace matching;

class ReferenceEngineTest : public ::testing::Test {
protected:
    ReferenceEngine engine_;

    IncomingOrder make_order(OrderID id, Side side, Price price, Quantity qty,
                             OrderType type = OrderType::Limit) {
        IncomingOrder o;
        o.id = id;
        o.side = side;
        o.type = type;
        o.price = price;
        o.quantity = qty;
        o.timestamp = id;
        return o;
    }
};

TEST_F(ReferenceEngineTest, RestingBuyOrder) {
    engine_.process_order(make_order(1, Side::Buy, 100, 100));
    EXPECT_EQ(engine_.best_bid(), 100);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, RestingSellOrder) {
    engine_.process_order(make_order(1, Side::Sell, 105, 100));
    EXPECT_EQ(engine_.best_ask(), 105);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, BuyCrossesSell) {
    engine_.process_order(make_order(1, Side::Sell, 105, 50));
    auto event = engine_.process_order(make_order(2, Side::Buy, 110, 30));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].price, 105);
    EXPECT_EQ(event.trades[0].quantity, 30);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, SellCrossesBuy) {
    engine_.process_order(make_order(1, Side::Buy, 100, 50));
    auto event = engine_.process_order(make_order(2, Side::Sell, 95, 30));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].price, 100);
    EXPECT_EQ(event.trades[0].quantity, 30);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, PriceTimePriority) {
    engine_.process_order(make_order(1, Side::Sell, 105, 20));
    engine_.process_order(make_order(2, Side::Sell, 105, 30));

    auto event = engine_.process_order(make_order(3, Side::Buy, 105, 25));
    EXPECT_EQ(event.trades.size(), 2u);
    EXPECT_EQ(event.trades[0].sell_order_id, 1u);
    EXPECT_EQ(event.trades[0].quantity, 20);
    EXPECT_EQ(event.trades[1].sell_order_id, 2u);
    EXPECT_EQ(event.trades[1].quantity, 5);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, FullFill) {
    engine_.process_order(make_order(1, Side::Sell, 105, 50));
    engine_.process_order(make_order(2, Side::Buy, 105, 50));
    EXPECT_EQ(engine_.best_ask(), 0);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, CancelOrder) {
    engine_.process_order(make_order(1, Side::Buy, 100, 100));
    auto event = engine_.cancel_order(1);
    EXPECT_FALSE(event.reports.empty());
    EXPECT_EQ(event.reports[0].status, OrderStatus::Cancelled);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, MarketOrder) {
    engine_.process_order(make_order(1, Side::Sell, 105, 100));
    auto event = engine_.process_order(make_order(2, Side::Buy, 0, 50, OrderType::Market));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].quantity, 50);
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, NoCrossNoFill) {
    engine_.process_order(make_order(1, Side::Sell, 105, 100));
    auto event = engine_.process_order(make_order(2, Side::Buy, 104, 50));
    EXPECT_TRUE(event.trades.empty());
    EXPECT_TRUE(engine_.verify_invariants());
}

TEST_F(ReferenceEngineTest, PartialFill) {
    engine_.process_order(make_order(1, Side::Sell, 105, 100));
    auto event = engine_.process_order(make_order(2, Side::Buy, 105, 30));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(engine_.resting_quantity(Side::Sell, 105), 70);
    EXPECT_TRUE(engine_.verify_invariants());
}
