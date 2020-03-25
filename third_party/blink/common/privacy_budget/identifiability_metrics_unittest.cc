// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"

#include "stdint.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(IdentifiabilityMetricsTest, DigestForMetrics_Basic) {
  const uint8_t kInput[] = {1, 2, 3, 4, 5};
  auto digest = DigestForMetrics(kInput);

  // Due to our requirement that the digest be stable and persistable, this test
  // should always pass once the code reaches the stable branch.
  EXPECT_EQ(UINT64_C(238255523), digest);
}

TEST(IdentifiabilityMetricsTest, DigestForMetrics_Padding) {
  const uint8_t kTwoBytes[] = {1, 2};
  const std::vector<uint8_t> kLong(16 * 1024, 'x');

  // Ideally we should be using all 64-bits or at least the 56 LSBs.
  EXPECT_EQ(UINT64_C(2790220116), DigestForMetrics(kTwoBytes));
  EXPECT_EQ(UINT64_C(2857827930), DigestForMetrics(kLong));
}

TEST(IdentifiabilityMetricsTest, DigestForMetrics_EdgeCases) {
  const std::vector<uint8_t> kEmpty;
  const uint8_t kOneByte[] = {1};

  // As before, these tests should always pass.
  EXPECT_EQ(0x0u, DigestForMetrics(kEmpty));
  EXPECT_EQ(UINT64_C(0x9e76b331), DigestForMetrics(kOneByte));
}

}  // namespace blink
