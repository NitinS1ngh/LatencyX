#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <bit>
#include <vector>

namespace matching {

// Dynamic bitset supporting O(1) find-highest and find-lowest via CLZ/CTZ.
class DynamicBitset {
public:
    explicit DynamicBitset(int num_bits)
        : num_bits_(num_bits)
        , num_words_((num_bits + 63) / 64)
        , data_(num_words_, 0)
    {}

    void set(int bit) {
        if (bit >= 0 && bit < num_bits_) {
            data_[bit >> 6] |= (1ULL << (bit & 63));
        }
    }

    void clear(int bit) {
        if (bit >= 0 && bit < num_bits_) {
            data_[bit >> 6] &= ~(1ULL << (bit & 63));
        }
    }

    bool test(int bit) const {
        if (bit < 0 || bit >= num_bits_) return false;
        return (data_[bit >> 6] >> (bit & 63)) & 1;
    }

    // Find the highest set bit. Returns -1 if empty.
    int find_highest() const {
        for (int i = num_words_ - 1; i >= 0; --i) {
            if (data_[i] != 0) {
                return (i << 6) + (63 - __builtin_clzll(data_[i]));
            }
        }
        return -1;
    }

    // Find the lowest set bit. Returns -1 if empty.
    int find_lowest() const {
        for (int i = 0; i < num_words_; ++i) {
            if (data_[i] != 0) {
                return (i << 6) + __builtin_ctzll(data_[i]);
            }
        }
        return -1;
    }

    bool empty() const {
        for (int i = 0; i < num_words_; ++i) {
            if (data_[i] != 0) return false;
        }
        return true;
    }

    void reset() {
        std::fill(data_.begin(), data_.end(), 0);
    }

    int num_bits() const { return num_bits_; }

private:
    int                   num_bits_;
    int                   num_words_;
    std::vector<uint64_t> data_;
};

} // namespace matching
