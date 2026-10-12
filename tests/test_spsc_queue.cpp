#include "matching/spsc_queue.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace matching;

TEST(SPSCQueueTest, BasicPushPop) {
    SPSCQueue<int, 16> q;
    EXPECT_TRUE(q.empty());

    EXPECT_TRUE(q.try_push(42));
    EXPECT_FALSE(q.empty());

    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueueTest, FIFOOrder) {
    SPSCQueue<int, 16> q;
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }

    for (int i = 0; i < 10; ++i) {
        int val = -1;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueueTest, FullQueue) {
    SPSCQueue<int, 4> q;
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_TRUE(q.try_push(4));
    EXPECT_FALSE(q.try_push(5)); // full

    int val;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(q.try_push(5)); // now there's room
}

TEST(SPSCQueueTest, Peek) {
    SPSCQueue<int, 16> q;
    EXPECT_TRUE(q.try_push(10));
    q.try_push(20);

    const int* p = q.peek();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 10);

    q.pop_commit();
    p = q.peek();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 20);

    q.pop_commit();
    EXPECT_EQ(q.peek(), nullptr);
}

TEST(SPSCQueueTest, SizeApprox) {
    SPSCQueue<int, 16> q;
    EXPECT_EQ(q.size_approx(), 0u);

    q.try_push(1);
    q.try_push(2);
    q.try_push(3);
    EXPECT_EQ(q.size_approx(), 3u);

    int val;
    q.try_pop(val);
    EXPECT_EQ(q.size_approx(), 2u);
}

TEST(SPSCQueueTest, ConcurrentProducerConsumer) {
    // Producer sends N items, consumer receives them in order.
    constexpr int N = 100000;
    SPSCQueue<int, 1024> q;

    std::atomic<bool> done_producing{false};

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!q.try_push(i)) {
                // spin — real systems would have backpressure
            }
        }
        done_producing.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        int received = 0;
        int expected = 0;
        while (received < N) {
            int val;
            if (q.try_pop(val)) {
                EXPECT_EQ(val, expected) << "Out of order at position " << received;
                ++expected;
                ++received;
            }
        }
    });

    producer.join();
    consumer.join();
}

TEST(SPSCQueueTest, HighThroughput) {
    // Stress test: producer and consumer running at max speed
    constexpr int N = 1000000;
    SPSCQueue<uint64_t, 4096> q;

    std::thread producer([&]() {
        for (uint64_t i = 0; i < static_cast<uint64_t>(N); ++i) {
            while (!q.try_push(i)) {}
        }
    });

    std::atomic<uint64_t> sum{0};
    std::thread consumer([&]() {
        uint64_t val;
        uint64_t local_sum = 0;
        int count = 0;
        while (count < N) {
            if (q.try_pop(val)) {
                local_sum += val;
                ++count;
            }
        }
        sum.store(local_sum, std::memory_order_relaxed);
    });

    producer.join();
    consumer.join();

    // Verify sum of 0..N-1
    uint64_t expected_sum = static_cast<uint64_t>(N) * (N - 1) / 2;
    EXPECT_EQ(sum.load(), expected_sum);
}
