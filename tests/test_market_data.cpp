#include "matching/market_data.h"
#include "matching/matching_engine.h"
#include <gtest/gtest.h>

using namespace matching;

class MarketDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<MatchingEngine>(20000, 100000);
        handler_ = std::make_unique<MarketDataHandler>();
    }

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

    std::unique_ptr<MatchingEngine>    engine_;
    std::unique_ptr<MarketDataHandler> handler_;
};

TEST_F(MarketDataTest, TopOfBookEmpty) {
    auto tob = MarketDataHandler::get_top_of_book(*engine_);
    EXPECT_EQ(tob.best_bid, 0);
    EXPECT_EQ(tob.best_ask, 0);
    EXPECT_EQ(tob.spread, 0);
}

TEST_F(MarketDataTest, TopOfBookWithOrders) {
    engine_->process_order(make_order(1, Side::Buy, 10000, 100));
    engine_->process_order(make_order(2, Side::Sell, 10005, 200));

    auto tob = MarketDataHandler::get_top_of_book(*engine_);
    EXPECT_EQ(tob.best_bid, 10000);
    EXPECT_EQ(tob.best_ask, 10005);
    EXPECT_EQ(tob.spread, 5);
    EXPECT_EQ(tob.bid_quantity, 100);
    EXPECT_EQ(tob.ask_quantity, 200);
}

TEST_F(MarketDataTest, OnMatchEventRecordsTrades) {
    engine_->process_order(make_order(1, Side::Sell, 10005, 50));
    auto event = engine_->process_order(make_order(2, Side::Buy, 10005, 30));

    handler_->on_match_event(event);
    EXPECT_EQ(handler_->recent_trades().size(), 1u);
    EXPECT_EQ(handler_->recent_trades()[0].quantity, 30);
}

TEST_F(MarketDataTest, TradeCallback) {
    std::vector<Trade> received_trades;
    handler_->set_trade_callback([&](const Trade& t) {
        received_trades.push_back(t);
    });

    engine_->process_order(make_order(1, Side::Sell, 10005, 50));
    auto event = engine_->process_order(make_order(2, Side::Buy, 10005, 30));
    handler_->on_match_event(event);

    EXPECT_EQ(received_trades.size(), 1u);
    EXPECT_EQ(received_trades[0].quantity, 30);
}

TEST_F(MarketDataTest, Clear) {
    engine_->process_order(make_order(1, Side::Sell, 10005, 50));
    auto event = engine_->process_order(make_order(2, Side::Buy, 10005, 30));
    handler_->on_match_event(event);

    EXPECT_FALSE(handler_->recent_trades().empty());
    handler_->clear();
    EXPECT_TRUE(handler_->recent_trades().empty());
    EXPECT_TRUE(handler_->recent_reports().empty());
}
