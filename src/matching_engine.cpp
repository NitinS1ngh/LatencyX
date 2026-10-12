#include "matching/matching_engine.h"
#include <cassert>
#include <algorithm>

namespace matching {

MatchingEngine::MatchingEngine(int max_price_levels, std::size_t pool_capacity)
    : pool_(pool_capacity)
    , bids_(max_price_levels > 0 ? max_price_levels : 20000)
    , asks_(max_price_levels > 0 ? max_price_levels : 20000)
    , tick_size_(1)
{
    // base_price=0: index = price / tick_size, so prices 0..max_levels-1 are valid
    bids_.set_base_price(0);
    asks_.set_base_price(0);
}

Price MatchingEngine::spread() const {
    Price bb = best_bid();
    Price ba = best_ask();
    if (bb == 0 || ba == 0) return 0;
    return ba - bb;
}

Quantity MatchingEngine::resting_quantity(Side side, Price price) const {
    const auto& ladder = (side == Side::Buy) ? bids_ : asks_;
    int idx = ladder.price_to_index(price);
    if (idx < 0) return 0;

    Quantity total = 0;
    ladder.level_at(idx).for_each([&](const Order* o) {
        total += o->remaining;
    });
    return total;
}

bool MatchingEngine::validate_order(const IncomingOrder& order) const {
    if (order.quantity <= 0) return false;
    if (order.price <= 0 && order.type == OrderType::Limit) return false;
    if (order.id == 0) return false;
    return true;
}

MatchEvent MatchingEngine::process_order(const IncomingOrder& order) {
    MatchEvent event;

    if (!validate_order(order)) {
        ExecutionReport report;
        report.order_id = order.id;
        report.status = OrderStatus::Cancelled;
        report.side = order.side;
        report.price = order.price;
        report.filled_quantity = 0;
        report.remaining_quantity = order.quantity;
        event.reports.push_back(report);
        return event;
    }

    IncomingOrder incoming = order;
    if (order.side == Side::Buy) {
        match_buy(incoming, event);
    } else {
        match_sell(incoming, event);
    }

    Quantity filled = order.quantity - incoming.quantity;
    bool should_rest = false;
    OrderStatus final_status;

    if (incoming.quantity == 0) {
        final_status = OrderStatus::Filled;
    } else if (filled > 0) {
        if (order.type == OrderType::IOC || order.type == OrderType::FOK) {
            final_status = OrderStatus::Cancelled;
        } else if (order.type == OrderType::Limit) {
            should_rest = true;
            final_status = OrderStatus::Resting;
        } else {
            final_status = OrderStatus::Cancelled;
        }
    } else {
        if (order.type == OrderType::Market || order.type == OrderType::IOC || order.type == OrderType::FOK) {
            final_status = OrderStatus::Cancelled;
        } else if (order.type == OrderType::Limit) {
            should_rest = true;
            final_status = OrderStatus::Resting;
        } else {
            final_status = OrderStatus::Cancelled;
        }
    }

    if (should_rest) {
        Order* pool_order = pool_.allocate();
        if (!pool_order) {
            final_status = OrderStatus::Cancelled;
            should_rest = false;
        } else {
            pool_order->id = order.id;
            pool_order->side = order.side;
            pool_order->type = order.type;
            pool_order->price = order.price;
            pool_order->quantity = order.quantity;
            pool_order->remaining = incoming.quantity;
            pool_order->timestamp = order.timestamp;
            pool_order->status = final_status;

            insert_order(pool_order);
            order_map_[pool_order->id] = pool_order;
        }
    }

    ExecutionReport report;
    report.order_id = order.id;
    report.status = final_status;
    report.side = order.side;
    report.price = order.price;
    report.filled_quantity = filled;
    report.remaining_quantity = incoming.quantity;
    event.reports.push_back(report);

    return event;
}

void MatchingEngine::match_buy(IncomingOrder& order, MatchEvent& event) {
    // Match against asks (lowest price first)
    while (order.quantity > 0) {
        int best_idx = asks_.find_best_ask();
        if (best_idx < 0) break;

        Price best_ask_price = asks_.index_to_price(best_idx);

        // Check if this buy order crosses the best ask
        bool crosses = false;
        if (order.type == OrderType::Market) {
            crosses = true;
        } else {
            crosses = (order.price >= best_ask_price);
        }
        if (!crosses) break;

        PriceLevelList& level = asks_.level_at(best_idx);

        // Match against resting orders at this level, FIFO order
        while (order.quantity > 0 && !level.empty()) {
            Order* resting = level.front();
            Quantity fill_qty = std::min(order.quantity, resting->remaining);

            // Generate trade
            Trade trade;
            trade.buy_order_id = order.id;
            trade.sell_order_id = resting->id;
            trade.price = resting->price; // price-time: resting order's price
            trade.quantity = fill_qty;
            trade.timestamp = order.timestamp;
            event.trades.push_back(trade);

            order.quantity -= fill_qty;
            resting->remaining -= fill_qty;

            if (resting->remaining == 0) {
                // Fully filled — remove from book
                level.remove(resting);
                asks_.mark_empty(best_idx);
                resting->status = OrderStatus::Filled;
                order_map_.erase(resting->id);

                // Generate execution report for the resting order
                ExecutionReport report;
                report.order_id = resting->id;
                report.status = OrderStatus::Filled;
                report.side = Side::Sell;
                report.price = resting->price;
                report.filled_quantity = resting->quantity;
                report.remaining_quantity = 0;
                event.reports.push_back(report);

                pool_.deallocate(resting);
            } else {
                // Partially filled — stays in book with same position
                resting->status = OrderStatus::PartiallyFilled;
            }
        }
    }
}

void MatchingEngine::match_sell(IncomingOrder& order, MatchEvent& event) {
    // Match against bids (highest price first)
    while (order.quantity > 0) {
        int best_idx = bids_.find_best_bid();
        if (best_idx < 0) break;

        Price best_bid_price = bids_.index_to_price(best_idx);

        // Check if this sell order crosses the best bid
        bool crosses = false;
        if (order.type == OrderType::Market) {
            crosses = true;
        } else {
            crosses = (order.price <= best_bid_price);
        }
        if (!crosses) break;

        PriceLevelList& level = bids_.level_at(best_idx);

        // Match against resting orders at this level, FIFO order
        while (order.quantity > 0 && !level.empty()) {
            Order* resting = level.front();
            Quantity fill_qty = std::min(order.quantity, resting->remaining);

            // Generate trade
            Trade trade;
            trade.buy_order_id = resting->id;
            trade.sell_order_id = order.id;
            trade.price = resting->price; // price-time: resting order's price
            trade.quantity = fill_qty;
            trade.timestamp = order.timestamp;
            event.trades.push_back(trade);

            order.quantity -= fill_qty;
            resting->remaining -= fill_qty;

            if (resting->remaining == 0) {
                // Fully filled — remove from book
                level.remove(resting);
                bids_.mark_empty(best_idx);
                resting->status = OrderStatus::Filled;
                order_map_.erase(resting->id);

                // Generate execution report for the resting order
                ExecutionReport report;
                report.order_id = resting->id;
                report.status = OrderStatus::Filled;
                report.side = Side::Buy;
                report.price = resting->price;
                report.filled_quantity = resting->quantity;
                report.remaining_quantity = 0;
                event.reports.push_back(report);

                pool_.deallocate(resting);
            } else {
                // Partially filled — stays in book with same position
                resting->status = OrderStatus::PartiallyFilled;
            }
        }
    }
}

void MatchingEngine::insert_order(Order* order) {
    PriceLadder& ladder = (order->side == Side::Buy) ? bids_ : asks_;
    int idx = ladder.price_to_index(order->price);
    if (idx < 0) {
        // Price out of range — can't insert
        order->status = OrderStatus::Cancelled;
        return;
    }

    ladder.level_at(idx).push_back(order);
    ladder.mark_occupied(idx);
    order_map_[order->id] = order;
}

void MatchingEngine::remove_order_from_book(Order* order) {
    PriceLadder& ladder = (order->side == Side::Buy) ? bids_ : asks_;
    int idx = ladder.price_to_index(order->price);
    if (idx < 0) return;

    ladder.level_at(idx).remove(order);
    ladder.mark_empty(idx);
    order_map_.erase(order->id);
}

MatchEvent MatchingEngine::cancel_order(OrderID id) {
    MatchEvent event;

    auto it = order_map_.find(id);
    if (it == order_map_.end()) {
        // Order not found — already filled or cancelled
        return event;
    }

    Order* order = it->second;
    remove_order_from_book(order);

    ExecutionReport report;
    report.order_id = order->id;
    report.status = OrderStatus::Cancelled;
    report.side = order->side;
    report.price = order->price;
    report.filled_quantity = order->quantity - order->remaining;
    report.remaining_quantity = order->remaining;
    event.reports.push_back(report);

    pool_.deallocate(order);
    return event;
}

bool MatchingEngine::verify_invariants() const {
    // Check no crossed book
    Price bb = best_bid();
    Price ba = best_ask();
    if (bb > 0 && ba > 0 && bb >= ba) return false;

    // Check price levels are non-empty iff marked in bitset
    if (!bids_.verify_invariants()) return false;
    if (!asks_.verify_invariants()) return false;

    // Check order map consistency
    for (auto& [id, order] : order_map_) {
        if (order->id != id) return false;
        if (order->status != OrderStatus::Resting &&
            order->status != OrderStatus::PartiallyFilled) return false;
    }

    return true;
}

} // namespace matching
