// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"

#include "stdint.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_Basic) {
  const uint8_t kInput[] = {1, 2, 3, 4, 5};
  auto digest = IdentifiabilityDigestOfBytes(kInput);

  // Due to our requirement that the digest be stable and persistable, this test
  // should always pass once the code reaches the stable branch.
  EXPECT_EQ(UINT64_C(238255523), digest);
}

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_Padding) {
  const uint8_t kTwoBytes[] = {1, 2};
  const std::vector<uint8_t> kLong(16 * 1024, 'x');

  // Ideally we should be using all 64-bits or at least the 56 LSBs.
  EXPECT_EQ(UINT64_C(2790220116), IdentifiabilityDigestOfBytes(kTwoBytes));
  EXPECT_EQ(UINT64_C(2857827930), IdentifiabilityDigestOfBytes(kLong));
}

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_EdgeCases) {
  const std::vector<uint8_t> kEmpty;
  const uint8_t kOneByte[] = {1};

  // As before, these tests should always pass.
  EXPECT_EQ(0x0u, IdentifiabilityDigestOfBytes(kEmpty));
  EXPECT_EQ(UINT64_C(0x9e76b331), IdentifiabilityDigestOfBytes(kOneByte));
}

TEST(IdentifiabilityMetricsTest, PassInt) {
  EXPECT_EQ(UINT64_C(5), IdentifiabilityDigestHelper(5));
}

TEST(IdentifiabilityMetricsTest, PassChar) {
  EXPECT_EQ(UINT64_C(97), IdentifiabilityDigestHelper('a'));
}

TEST(IdentifiabilityMetricsTest, PassBool) {
  EXPECT_EQ(UINT64_C(1), IdentifiabilityDigestHelper(true));
}

TEST(IdentifiabilityMetricsTest, PassLong) {
  EXPECT_EQ(UINT64_C(5), IdentifiabilityDigestHelper(5L));
}

TEST(IdentifiabilityMetricsTest, PassUint16) {
  EXPECT_EQ(UINT64_C(5), IdentifiabilityDigestHelper(static_cast<uint16_t>(5)));
}

TEST(IdentifiabilityMetricsTest, PassSizet) {
  EXPECT_EQ(UINT64_C(1), IdentifiabilityDigestHelper(sizeof(char)));
}

TEST(IdentifiabilityMetricsTest, PassFloat) {
  EXPECT_NE(UINT64_C(0), IdentifiabilityDigestHelper(5.0f));
}

TEST(IdentifiabilityMetricsTest, PassDouble) {
  EXPECT_NE(UINT64_C(0), IdentifiabilityDigestHelper(5.0));
}

// Use an arbitrary, large number to make accidental matches unlikely.
enum SimpleEnum { kSimpleValue = 2730421 };

TEST(IdentifiabilityMetricsTest, PassEnum) {
  EXPECT_EQ(UINT64_C(2730421), IdentifiabilityDigestHelper(kSimpleValue));
}

// Use an arbitrary, large number to make accidental matches unlikely.
enum Simple64Enum : uint64_t { kSimple64Value = 4983422 };

TEST(IdentifiabilityMetricsTest, PassEnum64) {
  EXPECT_EQ(UINT64_C(4983422), IdentifiabilityDigestHelper(kSimple64Value));
}

// Use an arbitrary, large number to make accidental matches unlikely.
enum class SimpleEnumClass { kSimpleValue = 3498249 };

TEST(IdentifiabilityMetricsTest, PassEnumClass) {
  EXPECT_EQ(UINT64_C(3498249),
            IdentifiabilityDigestHelper(SimpleEnumClass::kSimpleValue));
}

// Use an arbitrary, large number to make accidental matches unlikely.
enum class SimpleEnumClass64 : uint64_t { kSimple64Value = 4398372 };

TEST(IdentifiabilityMetricsTest, PassEnumClass64) {
  EXPECT_EQ(UINT64_C(4398372),
            IdentifiabilityDigestHelper(SimpleEnumClass64::kSimple64Value));
}

TEST(IdentifiabilityMetricsTest, PassSpan) {
  const int data[] = {1, 2, 3};
  EXPECT_EQ(UINT64_C(825881411),
            IdentifiabilityDigestHelper(base::make_span(data)));
}

TEST(IdentifiabilityMetricsTest, PassSpanDouble) {
  const double data[] = {1.0, 2.0, 3.0};
  EXPECT_EQ(UINT64_C(2487485222),
            IdentifiabilityDigestHelper(base::make_span(data)));
}

constexpr uint64_t kExpectedCombinationResult = 2636419788;

TEST(IdentifiabilityMetricsTest, Combination) {
  const int data[] = {1, 2, 3};
  EXPECT_EQ(kExpectedCombinationResult,
            IdentifiabilityDigestHelper(
                5, 'a', true, static_cast<uint16_t>(5), sizeof(char),
                kSimpleValue, kSimple64Value, SimpleEnumClass::kSimpleValue,
                SimpleEnumClass64::kSimple64Value, base::make_span(data)));
}

TEST(IdentifiabilityMetricsTest, CombinationWithFloats) {
  const int data[] = {1, 2, 3};
  const int data_doubles[] = {1.0, 2.0, 3.0};
  EXPECT_NE(kExpectedCombinationResult,
            IdentifiabilityDigestHelper(
                5, 'a', true, static_cast<uint16_t>(5), sizeof(char),
                kSimpleValue, kSimple64Value, SimpleEnumClass::kSimpleValue,
                SimpleEnumClass64::kSimple64Value, 5.0f, 5.0,
                base::make_span(data), base::make_span(data_doubles)));
}

}  // namespace blink
