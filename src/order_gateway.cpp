#include "matching/order_gateway.h"
#include <sstream>

namespace matching {

std::string OrderGateway::validate(const IncomingOrder& order) const {
    std::ostringstream errors;

    if (order.id == 0) {
        errors << "Order ID must be non-zero; ";
    }
    if (order.quantity <= 0) {
        errors << "Quantity must be positive; ";
    }
    if (order.price <= 0 && order.type == OrderType::Limit) {
        errors << "Limit order price must be positive; ";
    }
    if (order.type == OrderType::Market && order.price != 0) {
        errors << "Market orders should not have a price; ";
    }

    return errors.str();
}

} // namespace matching
