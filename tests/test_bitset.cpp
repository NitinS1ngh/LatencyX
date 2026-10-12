#include "matching/bitset.h"
#include <gtest/gtest.h>

using namespace matching;

TEST(BitsetTest, EmptyByDefault) {
    DynamicBitset bs(256);
    EXPECT_TRUE(bs.empty());
    EXPECT_EQ(bs.find_highest(), -1);
    EXPECT_EQ(bs.find_lowest(), -1);
}

TEST(BitsetTest, SetAndFind) {
    DynamicBitset bs(256);
    bs.set(0);
    EXPECT_FALSE(bs.empty());
    EXPECT_EQ(bs.find_highest(), 0);
    EXPECT_EQ(bs.find_lowest(), 0);
}

TEST(BitsetTest, MultipleBits) {
    DynamicBitset bs(256);
    bs.set(5);
    bs.set(100);
    bs.set(200);

    EXPECT_EQ(bs.find_highest(), 200);
    EXPECT_EQ(bs.find_lowest(), 5);
}

TEST(BitsetTest, Clear) {
    DynamicBitset bs(256);
    bs.set(50);
    bs.set(100);
    bs.clear(50);

    EXPECT_EQ(bs.find_highest(), 100);
    EXPECT_EQ(bs.find_lowest(), 100);

    bs.clear(100);
    EXPECT_TRUE(bs.empty());
}

TEST(BitsetTest, EdgeBits) {
    DynamicBitset bs(256);
    bs.set(0);   // lowest bit of first word
    bs.set(63);  // highest bit of first word
    bs.set(64);  // lowest bit of second word
    bs.set(255); // highest bit overall

    EXPECT_EQ(bs.find_highest(), 255);
    EXPECT_EQ(bs.find_lowest(), 0);
}

TEST(BitsetTest, Reset) {
    DynamicBitset bs(256);
    bs.set(10);
    bs.set(200);
    bs.reset();
    EXPECT_TRUE(bs.empty());
}

TEST(BitsetTest, TestBit) {
    DynamicBitset bs(256);
    bs.set(42);
    EXPECT_TRUE(bs.test(42));
    EXPECT_FALSE(bs.test(43));
    EXPECT_FALSE(bs.test(41));
}
