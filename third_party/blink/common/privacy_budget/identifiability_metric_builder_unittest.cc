// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

#include "base/metrics/ukm_source_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace blink {

TEST(IdentifiabilityMetricBuilderTest, Set) {
  IdentifiabilityMetricBuilder builder(base::UkmSourceId{});
  constexpr int64_t kInputHash = 2;
  constexpr int64_t kValue = 3;

  const auto kSurface = IdentifiableSurface::FromTypeAndInput(
      IdentifiableSurface::Type::kWebFeature, kInputHash);

  builder.Set(kSurface, kValue);
  auto entry = builder.TakeEntry();
  constexpr auto kEventHash = ukm::builders::Identifiability::kEntryNameHash;

  EXPECT_EQ(entry->event_hash, kEventHash);
  EXPECT_EQ(entry->metrics.size(), 1u);
  EXPECT_EQ(entry->metrics.begin()->first, kSurface.ToUkmMetricHash());
  EXPECT_EQ(entry->metrics.begin()->second, kValue);
}

TEST(IdentifiabilityMetricBuilderTest, BuilderOverload) {
  constexpr int64_t kValue = 3;
  constexpr int64_t kInputHash = 2;
  constexpr auto kSurface = IdentifiableSurface::FromTypeAndInput(
      IdentifiableSurface::Type::kWebFeature, kInputHash);

  const auto kSource = base::UkmSourceId::New();
  auto expected_entry =
      IdentifiabilityMetricBuilder(kSource).Set(kSurface, kValue).TakeEntry();

  // Yes, it seems cyclical, but this tests that the overloaded constructors for
  // IdentifiabilityMetricBuilder are equivalent.
  const ukm::SourceId kUkmSource = kSource.ToInt64();
  auto entry = IdentifiabilityMetricBuilder(kUkmSource)
                   .Set(kSurface, kValue)
                   .TakeEntry();

  EXPECT_EQ(expected_entry->source_id, entry->source_id);
}

TEST(IdentifiabilityMetricBuilderTest, SetWebfeature) {
  constexpr int64_t kValue = 3;
  constexpr int64_t kTestInput =
      static_cast<int64_t>(mojom::WebFeature::kEventSourceDocument);

  IdentifiabilityMetricBuilder builder(base::UkmSourceId{});
  builder.SetWebfeature(mojom::WebFeature::kEventSourceDocument, kValue);
  auto entry = builder.TakeEntry();

  // Only testing that using SetWebfeature(x,y) is equivalent to
  // .Set(IdentifiableSurface::FromTypeAndInput(kWebFeature, x), y);
  auto expected_entry =
      IdentifiabilityMetricBuilder(base::UkmSourceId{})
          .Set(IdentifiableSurface::FromTypeAndInput(
                   IdentifiableSurface::Type::kWebFeature, kTestInput),
               kValue)
          .TakeEntry();

  EXPECT_EQ(expected_entry->event_hash, entry->event_hash);
  ASSERT_EQ(entry->metrics.size(), 1u);
  EXPECT_EQ(entry->metrics, expected_entry->metrics);
}

}  // namespace blink
