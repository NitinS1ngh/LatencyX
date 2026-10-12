#include "matching/order.h"
#include <gtest/gtest.h>
#include <vector>

using namespace matching;

TEST(OrderPoolTest, BasicAllocateDeallocate) {
    OrderPool pool(10);
    Order* o1 = pool.allocate();
    ASSERT_NE(o1, nullptr);
    o1->id = 1;
    o1->quantity = 100;

    Order* o2 = pool.allocate();
    ASSERT_NE(o2, nullptr);
    EXPECT_NE(o1, o2);

    pool.deallocate(o1);
    Order* o3 = pool.allocate();
    // o3 should be the recycled o1
    EXPECT_EQ(o3, o1);
}

TEST(OrderPoolTest, ExhaustPool) {
    OrderPool pool(3);
    Order* o1 = pool.allocate();
    Order* o2 = pool.allocate();
    Order* o3 = pool.allocate();
    ASSERT_NE(o1, nullptr);
    ASSERT_NE(o2, nullptr);
    ASSERT_NE(o3, nullptr);

    Order* o4 = pool.allocate();
    EXPECT_EQ(o4, nullptr);

    pool.deallocate(o2);
    Order* o5 = pool.allocate();
    EXPECT_NE(o5, nullptr);
    EXPECT_EQ(o5, o2);
}

TEST(OrderPoolTest, AllAllocationsAreDistinct) {
    OrderPool pool(100);
    std::vector<Order*> orders;
    for (int i = 0; i < 100; ++i) {
        Order* o = pool.allocate();
        ASSERT_NE(o, nullptr) << "Failed at allocation " << i;
        o->id = static_cast<OrderID>(i);
        orders.push_back(o);
    }
    // All should be unique
    for (size_t i = 0; i < orders.size(); ++i) {
        for (size_t j = i + 1; j < orders.size(); ++j) {
            EXPECT_NE(orders[i], orders[j]);
        }
    }
}

TEST(OrderPoolTest, RecycleAllThenAllocateAll) {
    OrderPool pool(5);
    std::vector<Order*> orders;
    for (int i = 0; i < 5; ++i) {
        orders.push_back(pool.allocate());
    }
    for (auto* o : orders) {
        pool.deallocate(o);
    }
    // Should be able to allocate all 5 again
    for (int i = 0; i < 5; ++i) {
        EXPECT_NE(pool.allocate(), nullptr);
    }
    EXPECT_EQ(pool.allocate(), nullptr);
}

TEST(OrderPoolTest, PriceLevelListPushRemove) {
    OrderPool pool(10);
    PriceLevelList list;

    Order* o1 = pool.allocate();
    o1->id = 1;
    Order* o2 = pool.allocate();
    o2->id = 2;
    Order* o3 = pool.allocate();
    o3->id = 3;

    list.push_back(o1);
    list.push_back(o2);
    list.push_back(o3);

    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(list.front()->id, 1u);
    EXPECT_EQ(list.back()->id, 3u);

    list.remove(o2);
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.front()->id, 1u);
    EXPECT_EQ(list.back()->id, 3u);

    list.remove(o1);
    EXPECT_EQ(list.size(), 1u);
    EXPECT_EQ(list.front()->id, 3u);

    list.remove(o3);
    EXPECT_TRUE(list.empty());
}

TEST(OrderPoolTest, PriceLevelListRemoveMiddle) {
    OrderPool pool(10);
    PriceLevelList list;

    Order* o1 = pool.allocate(); o1->id = 1;
    Order* o2 = pool.allocate(); o2->id = 2;
    Order* o3 = pool.allocate(); o3->id = 3;

    list.push_back(o1);
    list.push_back(o2);
    list.push_back(o3);

    list.remove(o2);

    EXPECT_EQ(list.front()->id, 1u);
    EXPECT_EQ(list.front()->next->id, 3u);
    EXPECT_EQ(list.back()->id, 3u);
    EXPECT_EQ(list.back()->prev->id, 1u);
}

TEST(OrderPoolTest, PriceLevelListForEach) {
    OrderPool pool(5);
    PriceLevelList list;

    Order* o1 = pool.allocate(); o1->id = 10;
    Order* o2 = pool.allocate(); o2->id = 20;
    list.push_back(o1);
    list.push_back(o2);

    std::vector<OrderID> ids;
    list.for_each([&](Order* o) { ids.push_back(o->id); });

    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 10u);
    EXPECT_EQ(ids[1], 20u);
}
