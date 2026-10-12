#pragma once

#include "types.h"
#include "events.h"
#include "order.h"
#include "price_ladder.h"
#include "spsc_queue.h"
#include <unordered_map>
#include <vector>
#include <functional>

namespace matching {

// Single-threaded price-time priority matching engine.
class MatchingEngine {
public:
    explicit MatchingEngine(int max_price_levels = PriceLadder::DEFAULT_MAX_LEVELS,
                            std::size_t pool_capacity = 1000000);

    // Process a single incoming order. Returns trade events and
    // execution reports. This is the hot path.
    MatchEvent process_order(const IncomingOrder& order);

    // Cancel an order by ID. O(1) amortized.
    MatchEvent cancel_order(OrderID id);

    // Get current top-of-book
    Price best_bid() const { return bids_.best_bid_price(); }
    Price best_ask() const { return asks_.best_ask_price(); }
    Price spread() const;

    // Get total resting quantity at a price level
    Quantity resting_quantity(Side side, Price price) const;

    // Verify book invariants (for testing)
    bool verify_invariants() const;

    // Accessors for debugging/testing
    std::size_t pool_capacity() const { return pool_.capacity(); }
    std::size_t active_order_count() const { return order_map_.size(); }

private:
    // Match an incoming order against the opposite side of the book.
    void match_buy(IncomingOrder& order, MatchEvent& event);
    void match_sell(IncomingOrder& order, MatchEvent& event);

    // Insert a resting order into the book
    void insert_order(Order* order);

    // Remove an order from the book and return it to the pool
    void remove_order_from_book(Order* order);

    // Validate order before processing
    bool validate_order(const IncomingOrder& order) const;

    OrderPool          pool_;
    PriceLadder        bids_;      // buy side
    PriceLadder        asks_;      // sell side
    Price              tick_size_;

    // Order ID → Order* for O(1) cancel lookup
    std::unordered_map<OrderID, Order*> order_map_;
};

} // namespace matching
