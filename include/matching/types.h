#pragma once

#include <cstdint>
#include <functional>

namespace matching {

using OrderID   = uint64_t;
using Price     = int64_t;
using Quantity  = int64_t;
using Timestamp = uint64_t;

enum class Side : uint8_t {
    Buy  = 0,
    Sell = 1,
};

enum class OrderType : uint8_t {
    Limit     = 0,
    Market    = 1,
    IOC       = 2,  // Immediate-Or-Cancel
    FOK       = 3,  // Fill-Or-Kill
};

enum class OrderStatus : uint8_t {
    New              = 0,
    Acked            = 1,
    Resting          = 2,
    PartiallyFilled  = 3,
    Filled           = 4,
    Cancelled        = 5,
};

struct Trade {
    OrderID   buy_order_id;
    OrderID   sell_order_id;
    Price     price;
    Quantity  quantity;
    Timestamp timestamp;
};

struct ExecutionReport {
    OrderID     order_id;
    OrderStatus status;
    Side        side;
    Price       price;
    Quantity    filled_quantity;
    Quantity    remaining_quantity;
};

} // namespace matching
