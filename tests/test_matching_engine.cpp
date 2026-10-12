#include "matching/matching_engine.h"
#include <gtest/gtest.h>

using namespace matching;

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set base price to 10000, tick size 1
        // This gives us a range of [10000, 10256) for 256 levels
        engine_ = std::make_unique<MatchingEngine>(20000, 100000);
    }

    IncomingOrder make_order(OrderID id, Side side, Price price, Quantity qty,
                             OrderType type = OrderType::Limit) {
        IncomingOrder o;
        o.id = id;
        o.side = side;
        o.type = type;
        o.price = price;
        o.quantity = qty;
        o.timestamp = id; // use id as timestamp for simplicity
        return o;
    }

    std::unique_ptr<MatchingEngine> engine_;
};

TEST_F(MatchingEngineTest, RestingBuyOrder) {
    auto event = engine_->process_order(make_order(1, Side::Buy, 10000, 100));
    EXPECT_TRUE(event.trades.empty());
    EXPECT_EQ(engine_->best_bid(), 10000);
    EXPECT_EQ(engine_->active_order_count(), 1u);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, RestingSellOrder) {
    auto event = engine_->process_order(make_order(1, Side::Sell, 10005, 100));
    EXPECT_TRUE(event.trades.empty());
    EXPECT_EQ(engine_->best_ask(), 10005);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, BuyCrossesSell) {
    // Rest a sell at 10005
    engine_->process_order(make_order(1, Side::Sell, 10005, 50));

    // Buy at 10010 should match
    auto event = engine_->process_order(make_order(2, Side::Buy, 10010, 30));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].price, 10005); // maker's price
    EXPECT_EQ(event.trades[0].quantity, 30);
    EXPECT_EQ(event.trades[0].buy_order_id, 2u);
    EXPECT_EQ(event.trades[0].sell_order_id, 1u);

    // Sell order should still have 20 remaining
    EXPECT_EQ(engine_->resting_quantity(Side::Sell, 10005), 20);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, SellCrossesBuy) {
    // Rest a buy at 10000
    engine_->process_order(make_order(1, Side::Buy, 10000, 50));

    // Sell at 9995 should match
    auto event = engine_->process_order(make_order(2, Side::Sell, 9995, 30));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].price, 10000); // maker's price
    EXPECT_EQ(event.trades[0].quantity, 30);

    EXPECT_EQ(engine_->resting_quantity(Side::Buy, 10000), 20);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, PriceTimePriority) {
    // Place two sell orders at the same price
    engine_->process_order(make_order(1, Side::Sell, 10005, 20));
    engine_->process_order(make_order(2, Side::Sell, 10005, 30));

    // Buy order should match order 1 first (FIFO)
    auto event = engine_->process_order(make_order(3, Side::Buy, 10005, 25));
    EXPECT_EQ(event.trades.size(), 2u);
    EXPECT_EQ(event.trades[0].sell_order_id, 1u);
    EXPECT_EQ(event.trades[0].quantity, 20);
    EXPECT_EQ(event.trades[1].sell_order_id, 2u);
    EXPECT_EQ(event.trades[1].quantity, 5);

    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, MultiplePriceLevels) {
    // Place sells at multiple price levels
    engine_->process_order(make_order(1, Side::Sell, 10005, 10));
    engine_->process_order(make_order(2, Side::Sell, 10006, 10));
    engine_->process_order(make_order(3, Side::Sell, 10007, 10));

    // Buy at 10007 should match all three
    auto event = engine_->process_order(make_order(4, Side::Buy, 10007, 25));
    EXPECT_EQ(event.trades.size(), 3u);
    EXPECT_EQ(event.trades[0].price, 10005);
    EXPECT_EQ(event.trades[1].price, 10006);
    EXPECT_EQ(event.trades[2].price, 10007);
    EXPECT_EQ(event.trades[0].quantity, 10);
    EXPECT_EQ(event.trades[1].quantity, 10);
    EXPECT_EQ(event.trades[2].quantity, 5);

    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, MarketOrder) {
    engine_->process_order(make_order(1, Side::Sell, 10005, 100));

    auto event = engine_->process_order(make_order(2, Side::Buy, 0, 50, OrderType::Market));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].quantity, 50);
    EXPECT_EQ(engine_->resting_quantity(Side::Sell, 10005), 50);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, PartialFill) {
    engine_->process_order(make_order(1, Side::Sell, 10005, 100));

    auto event = engine_->process_order(make_order(2, Side::Buy, 10005, 30));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].quantity, 30);
    EXPECT_EQ(engine_->resting_quantity(Side::Sell, 10005), 70);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, CancelOrder) {
    engine_->process_order(make_order(1, Side::Buy, 10000, 100));
    EXPECT_EQ(engine_->best_bid(), 10000);

    auto event = engine_->cancel_order(1);
    EXPECT_FALSE(event.reports.empty());
    EXPECT_EQ(event.reports[0].status, OrderStatus::Cancelled);
    EXPECT_EQ(engine_->best_bid(), 0);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, CancelNonexistentOrder) {
    auto event = engine_->cancel_order(999);
    EXPECT_TRUE(event.reports.empty());
}

TEST_F(MatchingEngineTest, FullFill) {
    engine_->process_order(make_order(1, Side::Sell, 10005, 50));

    auto event = engine_->process_order(make_order(2, Side::Buy, 10005, 50));
    EXPECT_EQ(event.trades.size(), 1u);
    EXPECT_EQ(event.trades[0].quantity, 50);
    EXPECT_EQ(engine_->best_ask(), 0);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, NoCrossNoFill) {
    // Buy below the ask — no fill
    engine_->process_order(make_order(1, Side::Sell, 10005, 100));
    auto event = engine_->process_order(make_order(2, Side::Buy, 10004, 50));
    EXPECT_TRUE(event.trades.empty());
    EXPECT_EQ(engine_->resting_quantity(Side::Buy, 10004), 50);
    EXPECT_TRUE(engine_->verify_invariants());
}

TEST_F(MatchingEngineTest, InvalidOrderRejected) {
    auto event = engine_->process_order(make_order(0, Side::Buy, 10000, 100));
    EXPECT_FALSE(event.reports.empty());
    EXPECT_EQ(event.reports[0].status, OrderStatus::Cancelled);
}

TEST_F(MatchingEngineTest, SpreadCalculation) {
    engine_->process_order(make_order(1, Side::Buy, 10000, 100));
    engine_->process_order(make_order(2, Side::Sell, 10005, 100));
    EXPECT_EQ(engine_->spread(), 5);
}

TEST_F(MatchingEngineTest, SpreadEmptyBook) {
    EXPECT_EQ(engine_->spread(), 0);
}
