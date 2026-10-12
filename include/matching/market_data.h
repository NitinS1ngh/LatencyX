#pragma once

#include "types.h"
#include "matching_engine.h"
#include "spsc_queue.h"
#include <vector>
#include <functional>

namespace matching {

// Market data handler: trade notification and top-of-book dissemination.
class MarketDataHandler {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    MarketDataHandler() = default;

    // Process trade events from the matching engine
    void on_match_event(const MatchEvent& event) {
        for (const auto& trade : event.trades) {
            recent_trades_.push_back(trade);
            if (trade_callback_) {
                trade_callback_(trade);
            }
        }
        for (const auto& report : event.reports) {
            recent_reports_.push_back(report);
        }
    }

    // Access recent events (for testing)
    const std::vector<Trade>& recent_trades() const { return recent_trades_; }
    const std::vector<ExecutionReport>& recent_reports() const { return recent_reports_; }

    void clear() {
        recent_trades_.clear();
        recent_reports_.clear();
    }

    // Set a callback for real-time trade notifications
    void set_trade_callback(TradeCallback cb) {
        trade_callback_ = std::move(cb);
    }

    // Get top-of-book snapshot
    struct TopOfBook {
        Price best_bid = 0;
        Quantity bid_quantity = 0;
        Price best_ask = 0;
        Quantity ask_quantity = 0;
        Price spread = 0;
    };

    static TopOfBook get_top_of_book(const MatchingEngine& engine) {
        TopOfBook tob;
        tob.best_bid = engine.best_bid();
        tob.best_ask = engine.best_ask();
        tob.spread = engine.spread();
        tob.bid_quantity = engine.resting_quantity(Side::Buy, tob.best_bid);
        tob.ask_quantity = engine.resting_quantity(Side::Sell, tob.best_ask);
        return tob;
    }

private:
    std::vector<Trade>           recent_trades_;
    std::vector<ExecutionReport> recent_reports_;
    TradeCallback                trade_callback_;
};

} // namespace matching
