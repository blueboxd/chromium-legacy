// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

#include "base/metrics/ukm_source_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

TEST(IdentifiabilityMetricBuilderTest, Set) {
  IdentifiabilityMetricBuilder builder(base::UkmSourceId{});
  constexpr int64_t kTestInputHash = 2;
  constexpr int64_t kTestValue = 3;

  auto surface = IdentifiableSurface::FromTypeAndInput(
      IdentifiableSurface::Type::kWebFeature, kTestInputHash);

  builder.Set(surface, kTestValue);
  auto entry = builder.TakeEntry();
  constexpr auto kEventHash = ukm::builders::Identifiability::kEntryNameHash;

  EXPECT_EQ(entry->event_hash, kEventHash);
  EXPECT_EQ(entry->metrics.size(), 1u);
  EXPECT_EQ(entry->metrics.begin()->first, surface.ToUkmMetricHash());
  EXPECT_EQ(entry->metrics.begin()->second, kTestValue);
}

}  // namespace blink
