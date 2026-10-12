#pragma once

#include "order.h"
#include "bitset.h"
#include <vector>
#include <cassert>

namespace matching {

// Array-indexed price ladder mapping discretized tick prices to levels.
// Bitset provides O(1) best bid/ask discovery via hardware CLZ/CTZ.

class PriceLadder {
public:
    // max_levels: total number of price levels (both sides).
    // Must be >= max expected price if base_price=0.
    static constexpr int DEFAULT_MAX_LEVELS = 20000;

    explicit PriceLadder(int max_levels = DEFAULT_MAX_LEVELS)
        : max_levels_(max_levels)
        , levels_(max_levels)
        , occupied_bits_(max_levels)
        , base_price_(0)
        , tick_size_(1)
    {
    }

    // Set the center of the ladder. All prices within ±max_levels_/2
    // ticks of this base price can be accessed in O(1).
    void set_base_price(Price base) { base_price_ = base; }
    Price base_price() const { return base_price_; }

    void set_tick_size(Price tick) { tick_size_ = tick; }
    Price tick_size() const { return tick_size_; }

    // Convert price to index. Returns -1 if out of range.
    int price_to_index(Price price) const {
        if (tick_size_ == 0) return -1;
        Price offset = (price - base_price_) / tick_size_;
        if (offset < 0 || offset >= max_levels_) return -1;
        return static_cast<int>(offset);
    }

    // Convert index back to price
    Price index_to_price(int index) const {
        return base_price_ + static_cast<Price>(index) * tick_size_;
    }

    // Get the price level list at a given index
    PriceLevelList& level_at(int index) {
        assert(index >= 0 && index < max_levels_);
        return levels_[index];
    }

    const PriceLevelList& level_at(int index) const {
        assert(index >= 0 && index < max_levels_);
        return levels_[index];
    }

    // Register/unregister a price level in the bitset
    void mark_occupied(int index) { occupied_bits_.set(index); }
    void mark_empty(int index) {
        if (levels_[index].empty()) {
            occupied_bits_.clear(index);
        }
    }

    // Find best bid (highest occupied level on bid side)
    int find_best_bid() const { return occupied_bits_.find_highest(); }

    // Find best ask (lowest occupied level on ask side)
    int find_best_ask() const { return occupied_bits_.find_lowest(); }

    // Get best bid price, or 0 if empty
    Price best_bid_price() const {
        int idx = find_best_bid();
        return idx >= 0 ? index_to_price(idx) : 0;
    }

    // Get best ask price, or 0 if empty
    Price best_ask_price() const {
        int idx = find_best_ask();
        return idx >= 0 ? index_to_price(idx) : 0;
    }

    bool empty() const { return occupied_bits_.empty(); }
    int max_levels() const { return max_levels_; }

    // Debug: verify invariants
    bool verify_invariants() const;

private:
    int                  max_levels_;
    std::vector<PriceLevelList> levels_;
    DynamicBitset        occupied_bits_;
    Price                base_price_;
    Price                tick_size_;
};

} // namespace matching
