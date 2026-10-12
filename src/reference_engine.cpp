#include "matching/reference_engine.h"
#include <algorithm>
#include <cassert>

namespace matching {

Price ReferenceEngine::best_bid() const {
    if (bids_.empty()) return 0;
    return bids_.begin()->first;
}

Price ReferenceEngine::best_ask() const {
    if (asks_.empty()) return 0;
    return asks_.begin()->first;
}

Price ReferenceEngine::spread() const {
    Price bb = best_bid();
    Price ba = best_ask();
    if (bb == 0 || ba == 0) return 0;
    return ba - bb;
}

Quantity ReferenceEngine::resting_quantity(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) return 0;
        Quantity total = 0;
        for (const auto& o : it->second) {
            if (o.status != OrderStatus::Cancelled && o.status != OrderStatus::Filled) {
                total += o.remaining;
            }
        }
        return total;
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) return 0;
        Quantity total = 0;
        for (const auto& o : it->second) {
            if (o.status != OrderStatus::Cancelled && o.status != OrderStatus::Filled) {
                total += o.remaining;
            }
        }
        return total;
    }
}

MatchEvent ReferenceEngine::process_order(const IncomingOrder& order) {
    MatchEvent event;

    if (order.quantity <= 0 || order.id == 0) {
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

    // Determine final status
    Quantity filled = order.quantity - incoming.quantity;
    bool should_rest = false;

    if (incoming.quantity == 0) {
        // Fully filled
        ExecutionReport report;
        report.order_id = order.id;
        report.status = OrderStatus::Filled;
        report.side = order.side;
        report.price = order.price;
        report.filled_quantity = order.quantity;
        report.remaining_quantity = 0;
        event.reports.push_back(report);
    } else if (filled > 0) {
        // Partially filled
        if (order.type == OrderType::IOC || order.type == OrderType::FOK || order.type == OrderType::Market) {
            ExecutionReport report;
            report.order_id = order.id;
            report.status = OrderStatus::Cancelled;
            report.side = order.side;
            report.price = order.price;
            report.filled_quantity = filled;
            report.remaining_quantity = incoming.quantity;
            event.reports.push_back(report);
        } else if (order.type == OrderType::Limit) {
            should_rest = true;
        }
    } else {
        // No fill
        if (order.type == OrderType::Market) {
            ExecutionReport report;
            report.order_id = order.id;
            report.status = OrderStatus::Cancelled;
            report.side = order.side;
            report.price = order.price;
            report.filled_quantity = 0;
            report.remaining_quantity = order.quantity;
            event.reports.push_back(report);
        } else if (order.type == OrderType::IOC) {
            ExecutionReport report;
            report.order_id = order.id;
            report.status = OrderStatus::Cancelled;
            report.side = order.side;
            report.price = order.price;
            report.filled_quantity = 0;
            report.remaining_quantity = order.quantity;
            event.reports.push_back(report);
        } else if (order.type == OrderType::FOK) {
            ExecutionReport report;
            report.order_id = order.id;
            report.status = OrderStatus::Cancelled;
            report.side = order.side;
            report.price = order.price;
            report.filled_quantity = 0;
            report.remaining_quantity = order.quantity;
            event.reports.push_back(report);
        } else {
            should_rest = true;
        }
    }

    if (should_rest) {
        RefOrder ref_order;
        ref_order.id = order.id;
        ref_order.side = order.side;
        ref_order.type = order.type;
        ref_order.price = order.price;
        ref_order.quantity = order.quantity;
        ref_order.remaining = incoming.quantity;
        ref_order.timestamp = order.timestamp;
        ref_order.status = OrderStatus::Resting;
        insert_order(ref_order);

        ExecutionReport report;
        report.order_id = order.id;
        report.status = OrderStatus::Resting;
        report.side = order.side;
        report.price = order.price;
        report.filled_quantity = filled;
        report.remaining_quantity = incoming.quantity;
        event.reports.push_back(report);
    }

    return event;
}

void ReferenceEngine::match_buy(IncomingOrder& order, MatchEvent& event) {
    // Match against asks (lowest price first — std::map is ascending by default)
    for (auto ask_it = asks_.begin();
         ask_it != asks_.end() && order.quantity > 0; ) {
        Price ask_price = ask_it->first;

        // Check crossing
        bool crosses = false;
        if (order.type == OrderType::Market) {
            crosses = true;
        } else {
            crosses = (order.price >= ask_price);
        }
        if (!crosses) break;

        auto& level = ask_it->second;

        for (auto rest_it = level.begin();
             rest_it != level.end() && order.quantity > 0; ) {
            RefOrder& resting = *rest_it;
            if (resting.status == OrderStatus::Filled ||
                resting.status == OrderStatus::Cancelled) {
                rest_it = level.erase(rest_it);
                continue;
            }

            Quantity fill_qty = std::min(order.quantity, resting.remaining);

            Trade trade;
            trade.buy_order_id = order.id;
            trade.sell_order_id = resting.id;
            trade.price = resting.price;
            trade.quantity = fill_qty;
            trade.timestamp = order.timestamp;
            event.trades.push_back(trade);

            order.quantity -= fill_qty;
            resting.remaining -= fill_qty;

            if (resting.remaining == 0) {
                resting.status = OrderStatus::Filled;
                order_map_.erase(resting.id);

                ExecutionReport report;
                report.order_id = resting.id;
                report.status = OrderStatus::Filled;
                report.side = Side::Sell;
                report.price = resting.price;
                report.filled_quantity = resting.quantity;
                report.remaining_quantity = 0;
                event.reports.push_back(report);

                rest_it = level.erase(rest_it);
            } else {
                resting.status = OrderStatus::PartiallyFilled;
                ++rest_it;
            }
        }

        if (level.empty()) {
            ask_it = asks_.erase(ask_it);
        } else {
            ++ask_it;
        }
    }
}

void ReferenceEngine::match_sell(IncomingOrder& order, MatchEvent& event) {
    // Match against bids (highest price first)
    for (auto bid_it = bids_.begin();
         bid_it != bids_.end() && order.quantity > 0; ) {
        Price bid_price = bid_it->first;

        bool crosses = false;
        if (order.type == OrderType::Market) {
            crosses = true;
        } else {
            crosses = (order.price <= bid_price);
        }
        if (!crosses) break;

        auto& level = bid_it->second;

        for (auto rest_it = level.begin();
             rest_it != level.end() && order.quantity > 0; ) {
            RefOrder& resting = *rest_it;
            if (resting.status == OrderStatus::Filled ||
                resting.status == OrderStatus::Cancelled) {
                rest_it = level.erase(rest_it);
                continue;
            }

            Quantity fill_qty = std::min(order.quantity, resting.remaining);

            Trade trade;
            trade.buy_order_id = resting.id;
            trade.sell_order_id = order.id;
            trade.price = resting.price;
            trade.quantity = fill_qty;
            trade.timestamp = order.timestamp;
            event.trades.push_back(trade);

            order.quantity -= fill_qty;
            resting.remaining -= fill_qty;

            if (resting.remaining == 0) {
                resting.status = OrderStatus::Filled;
                order_map_.erase(resting.id);

                ExecutionReport report;
                report.order_id = resting.id;
                report.status = OrderStatus::Filled;
                report.side = Side::Buy;
                report.price = resting.price;
                report.filled_quantity = resting.quantity;
                report.remaining_quantity = 0;
                event.reports.push_back(report);

                rest_it = level.erase(rest_it);
            } else {
                resting.status = OrderStatus::PartiallyFilled;
                ++rest_it;
            }
        }

        if (level.empty()) {
            bid_it = bids_.erase(bid_it);
        } else {
            ++bid_it;
        }
    }
}

void ReferenceEngine::insert_order(const RefOrder& order) {
    if (order.side == Side::Buy) {
        bids_[order.price].push_back(order);
        order_map_[order.id] = &bids_[order.price].back();
    } else {
        asks_[order.price].push_back(order);
        order_map_[order.id] = &asks_[order.price].back();
    }
}

MatchEvent ReferenceEngine::cancel_order(OrderID id) {
    MatchEvent event;

    auto it = order_map_.find(id);
    if (it == order_map_.end()) return event;

    RefOrder* order = it->second;
    order->status = OrderStatus::Cancelled;

    ExecutionReport report;
    report.order_id = order->id;
    report.status = OrderStatus::Cancelled;
    report.side = order->side;
    report.price = order->price;
    report.filled_quantity = order->quantity - order->remaining;
    report.remaining_quantity = order->remaining;
    event.reports.push_back(report);

    order_map_.erase(it);
    return event;
}

bool ReferenceEngine::verify_invariants() const {
    // No crossed book
    Price bb = best_bid();
    Price ba = best_ask();
    if (bb > 0 && ba > 0 && bb >= ba) return false;

    // Ask side ascending
    Price prev = 0;
    for (auto& [price, level] : asks_) {
        if (price <= prev) return false;
        prev = price;
    }

    // Bid side descending
    prev = INT64_MAX;
    for (auto& [price, level] : bids_) {
        if (price >= prev) return false;
        prev = price;
    }

    return true;
}

} // namespace matching
