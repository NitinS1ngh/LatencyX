#pragma once

#include "types.h"
#include "events.h"
#include <map>
#include <list>
#include <unordered_map>
#include <vector>

namespace matching {

// Reference matching engine for differential verification.
class ReferenceEngine {
public:
    ReferenceEngine() = default;

    MatchEvent process_order(const IncomingOrder& order);
    MatchEvent cancel_order(OrderID id);

    Price best_bid() const;
    Price best_ask() const;
    Price spread() const;

    Quantity resting_quantity(Side side, Price price) const;
    bool verify_invariants() const;

private:
    struct RefOrder {
        OrderID     id;
        Side        side;
        OrderType   type;
        Price       price;
        Quantity    quantity;
        Quantity    remaining;
        Timestamp   timestamp;
        OrderStatus status;
    };

    // Bids: sorted descending by price (highest first)
    // Asks: sorted ascending by price (lowest first)
    struct DescendingPrice {
        bool operator()(Price a, Price b) const { return a > b; }
    };

    std::map<Price, std::list<RefOrder>, std::less<Price>>   asks_;
    std::map<Price, std::list<RefOrder>, DescendingPrice>    bids_;
    std::unordered_map<OrderID, RefOrder*>                   order_map_;

    void match_buy(IncomingOrder& order, MatchEvent& event);
    void match_sell(IncomingOrder& order, MatchEvent& event);
    void insert_order(const RefOrder& order);
};

} // namespace matching
