#pragma once

#include "types.h"
#include "events.h"
#include "spsc_queue.h"
#include "matching_engine.h"
#include <atomic>
#include <string>

namespace matching {

// Ingress order gateway: validates incoming orders and assigns sequence numbers.
class OrderGateway {
public:
    explicit OrderGateway(SPSCQueue<IncomingOrder, 65536>& queue)
        : queue_(queue)
        , next_sequence_(1)
    {}

    // Validate an incoming order. Returns an error string if invalid,
    // empty string if valid.
    std::string validate(const IncomingOrder& order) const;

    // Submit a validated order to the queue. Returns true on success.
    bool submit(const IncomingOrder& order) {
        IncomingOrder seq_order = order;
        seq_order.timestamp = next_sequence_.fetch_add(1, std::memory_order_relaxed);
        return queue_.try_push(seq_order);
    }

    // Submit with a callback that receives the execution report.
    // (Synchronous path for testing — no queue involved.)
    MatchEvent submit_direct(MatchingEngine& engine, const IncomingOrder& order) {
        IncomingOrder seq_order = order;
        seq_order.timestamp = next_sequence_.fetch_add(1, std::memory_order_relaxed);
        return engine.process_order(seq_order);
    }

    uint64_t sequence_number() const {
        return next_sequence_.load(std::memory_order_relaxed);
    }

private:
    SPSCQueue<IncomingOrder, 65536>& queue_;
    std::atomic<uint64_t>           next_sequence_;
};

} // namespace matching
