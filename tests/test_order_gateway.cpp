#include "matching/order_gateway.h"
#include "matching/matching_engine.h"
#include <gtest/gtest.h>

using namespace matching;

class OrderGatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<MatchingEngine>(20000, 100000);
        gateway_ = std::make_unique<OrderGateway>(queue_);
    }

    IncomingOrder make_order(OrderID id, Side side, Price price, Quantity qty,
                             OrderType type = OrderType::Limit) {
        IncomingOrder o;
        o.id = id;
        o.side = side;
        o.type = type;
        o.price = price;
        o.quantity = qty;
        o.timestamp = 0;
        return o;
    }

    SPSCQueue<IncomingOrder, 65536> queue_;
    std::unique_ptr<MatchingEngine> engine_;
    std::unique_ptr<OrderGateway>   gateway_;
};

TEST_F(OrderGatewayTest, ValidOrderPassesValidation) {
    auto order = make_order(1, Side::Buy, 10000, 100);
    EXPECT_TRUE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, ZeroIdFailsValidation) {
    auto order = make_order(0, Side::Buy, 10000, 100);
    EXPECT_FALSE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, ZeroQuantityFailsValidation) {
    auto order = make_order(1, Side::Buy, 10000, 0);
    EXPECT_FALSE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, NegativeQuantityFailsValidation) {
    auto order = make_order(1, Side::Buy, 10000, -5);
    EXPECT_FALSE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, ZeroPriceLimitFailsValidation) {
    auto order = make_order(1, Side::Buy, 0, 100, OrderType::Limit);
    EXPECT_FALSE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, MarketOrderWithPriceFailsValidation) {
    auto order = make_order(1, Side::Buy, 5000, 100, OrderType::Market);
    EXPECT_FALSE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, MarketOrderWithoutPricePassesValidation) {
    auto order = make_order(1, Side::Buy, 0, 100, OrderType::Market);
    EXPECT_TRUE(gateway_->validate(order).empty());
}

TEST_F(OrderGatewayTest, SequenceNumbers) {
    EXPECT_EQ(gateway_->sequence_number(), 1u);

    auto order1 = make_order(1, Side::Buy, 10000, 100);
    gateway_->submit(order1);
    EXPECT_EQ(gateway_->sequence_number(), 2u);

    auto order2 = make_order(2, Side::Sell, 10005, 50);
    gateway_->submit(order2);
    EXPECT_EQ(gateway_->sequence_number(), 3u);
}

TEST_F(OrderGatewayTest, SubmitDirect) {
    auto order = make_order(1, Side::Sell, 10005, 100);
    auto event = gateway_->submit_direct(*engine_, order);
    EXPECT_TRUE(event.trades.empty());
    EXPECT_EQ(engine_->best_ask(), 10005);
}

TEST_F(OrderGatewayTest, SubmitToQueue) {
    auto order = make_order(1, Side::Buy, 10000, 100);
    EXPECT_TRUE(gateway_->submit(order));

    IncomingOrder received;
    EXPECT_TRUE(queue_.try_pop(received));
    EXPECT_EQ(received.id, 1u);
    EXPECT_EQ(received.side, Side::Buy);
    EXPECT_EQ(received.timestamp, 1u); // sequence number assigned
}
