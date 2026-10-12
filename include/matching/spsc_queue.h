#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

namespace matching {

// Lock-free Single-Producer Single-Consumer circular ring buffer.
template<typename T, std::size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    SPSCQueue() : buffer_{}, head_(0), tail_(0) {}

    // Producer: try to push. Returns true if successful, false if full.
    bool try_push(const T& item) noexcept {
        const auto t = tail_.load(std::memory_order_relaxed);
        const auto h = head_.load(std::memory_order_acquire);
        if (t - h >= Capacity) return false;

        buffer_[t & mask()] = item;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    // Consumer: try to pop. Returns true if successful, false if empty.
    bool try_pop(T& item) noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_acquire);
        if (h >= t) return false;

        item = buffer_[h & mask()];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // Consumer: peek without removing. Returns nullptr if empty.
    const T* peek() const noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_acquire);
        if (h >= t) return nullptr;
        return &buffer_[h & mask()];
    }

    // Consumer: commit after peek
    void pop_commit() noexcept {
        head_.store(head_.load(std::memory_order_relaxed) + 1,
                    std::memory_order_release);
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) >=
               tail_.load(std::memory_order_acquire);
    }

    std::size_t size_approx() const noexcept {
        auto t = tail_.load(std::memory_order_acquire);
        auto h = head_.load(std::memory_order_acquire);
        return (t >= h) ? static_cast<std::size_t>(t - h) : 0;
    }

private:
    static constexpr std::size_t mask() { return Capacity - 1; }

    // Cache-line pad head and tail to prevent false sharing.
    // head_ is written only by consumer, tail_ only by producer.
    alignas(64) T buffer_[Capacity];
    alignas(64) std::atomic<uint64_t> head_;
    alignas(64) std::atomic<uint64_t> tail_;
};

} // namespace matching
