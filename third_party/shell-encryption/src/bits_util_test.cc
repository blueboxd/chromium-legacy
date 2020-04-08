#include "bits_util.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Eq;

namespace {

TEST(BitsUtilTest, CountOnes64Works) {
  EXPECT_THAT(rlwe::internal::CountOnes64(0xFF000000000000FF), Eq(16));
  EXPECT_THAT(rlwe::internal::CountOnes64(0xFF000000000000FE), Eq(15));
  EXPECT_THAT(rlwe::internal::CountOnes64(0xFF0000000000FF00), Eq(16));
  EXPECT_THAT(rlwe::internal::CountOnes64(0x1111111111111111), Eq(16));
  EXPECT_THAT(rlwe::internal::CountOnes64(0x0321212121212121), Eq(16));
}

TEST(BitsUtilTest, CountOnesInByte) {
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x00), Eq(0));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x01), Eq(1));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x11), Eq(2));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x22), Eq(2));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x44), Eq(2));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0xFF), Eq(8));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0xEE), Eq(6));
}

TEST(BitsUtilTest, CountLeadingZeros64Works) {
  rlwe::Uint64 value = 0x8000000000000000;
  for (int i = 0; i < 64; i++) {
    EXPECT_THAT(rlwe::internal::CountLeadingZeros64(value), Eq(i));
    value >>= 1;
  }
}

}  // namespace
