#pragma once

#include "types.h"
#include <cstddef>
#include <cstring>
#include <new>
#include <vector>

namespace matching {

// Order representation with intrusive list links for price levels.
struct Order {
    OrderID     id;
    Side        side;
    OrderType   type;
    Price       price;
    Quantity    quantity;
    Quantity    remaining;
    Timestamp   timestamp;
    OrderStatus status;

    // Intrusive pointers for price level queue (FIFO)
    Order*      prev = nullptr;
    Order*      next = nullptr;
    uint32_t    pool_index = 0;

    bool is_fully_filled() const { return remaining == 0; }
    bool is_marketable() const { return type == OrderType::Market; }

    bool would_cross_buy(Price best_ask) const {
        return side == Side::Buy && (type == OrderType::Market || price >= best_ask);
    }
    bool would_cross_sell(Price best_bid) const {
        return side == Side::Sell && (type == OrderType::Market || price <= best_bid);
    }
};

// Intrusive doubly-linked list for FIFO price levels.
class PriceLevelList {
public:
    PriceLevelList() = default;

    void push_back(Order* order) {
        order->prev = tail_;
        order->next = nullptr;
        if (tail_) {
            tail_->next = order;
        } else {
            head_ = order;
        }
        tail_ = order;
        ++size_;
    }

    void remove(Order* order) {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head_ = order->next;
        }
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail_ = order->prev;
        }
        order->prev = nullptr;
        order->next = nullptr;
        --size_;
    }

    Order* front() const { return head_; }
    Order* back() const { return tail_; }
    bool empty() const { return head_ == nullptr; }
    std::size_t size() const { return size_; }

    template<typename Fn>
    void for_each(Fn fn) const {
        for (Order* o = head_; o; o = o->next) {
            fn(o);
        }
    }

private:
    Order*      head_ = nullptr;
    Order*      tail_ = nullptr;
    std::size_t size_ = 0;
};

// Pre-allocated memory pool with freelist for O(1) alloc/dealloc.
class OrderPool {
public:
    explicit OrderPool(std::size_t capacity)
        : capacity_(capacity)
        , pool_(capacity)
        , free_head_(0)
    {
        for (std::size_t i = 0; i < capacity_ - 1; ++i) {
            pool_[i].pool_index = static_cast<uint32_t>(i + 1);
        }
        pool_[capacity_ - 1].pool_index = UINT32_MAX;
    }

    Order* allocate() {
        if (free_head_ == UINT32_MAX) return nullptr;

        uint32_t idx = free_head_;
        free_head_ = pool_[idx].pool_index;

        Order* order = &pool_[idx];
        order->pool_index = idx;
        order->prev = nullptr;
        order->next = nullptr;
        return order;
    }

    void deallocate(Order* order) {
        uint32_t idx = order->pool_index;
        order->pool_index = free_head_;
        free_head_ = idx;
    }

    std::size_t capacity() const { return capacity_; }

private:
    std::size_t       capacity_;
    std::vector<Order> pool_;
    uint32_t          free_head_;
};

} // namespace matching
