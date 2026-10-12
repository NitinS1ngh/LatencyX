#pragma once

#include "types.h"
#include <vector>

namespace matching {

// Input order from the gateway
struct IncomingOrder {
    OrderID     id;
    Side        side;
    OrderType   type;
    Price       price;
    Quantity    quantity;
    Timestamp   timestamp;
};

// Output trade event
struct MatchEvent {
    std::vector<Trade> trades;
    std::vector<ExecutionReport> reports;
};

} // namespace matching
