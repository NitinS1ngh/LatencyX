#include "matching/price_ladder.h"
#include <cassert>

namespace matching {

bool PriceLadder::verify_invariants() const {
    // Check that every occupied bit corresponds to a non-empty level
    for (int i = 0; i < max_levels_; ++i) {
        bool is_occupied = occupied_bits_.test(i);
        bool is_nonempty = !levels_[i].empty();
        if (is_occupied != is_nonempty) return false;
    }

    // Check FIFO ordering within each level (orders are in arrival order)
    for (int i = 0; i < max_levels_; ++i) {
        levels_[i].for_each([&](const Order* o) {
            if (o->pool_index == 0) return; // skip invalid
            if (o->remaining <= 0 && o->status != OrderStatus::Filled) return;
        });
    }

    return true;
}

} // namespace matching
