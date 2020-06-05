// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

#include <cinttypes>
#include <limits>

#include "base/metrics/ukm_source_id.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
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

namespace {

MATCHER_P(FirstMetricIs,
          entry,
          base::StringPrintf("entry is %s0x%" PRIx64,
                             negation ? "not " : "",
                             entry)) {
  return arg->metrics.begin()->second == entry;
}  // namespace

enum class Never { kGonna, kGive, kYou, kUp };

constexpr IdentifiableSurface kTestSurface =
    IdentifiableSurface::FromTypeAndInput(
        IdentifiableSurface::Type::kReservedInternal,
        0);

// Sample values
const char kAbcd[] = "abcd";
const int64_t kExpectedHashOfAbcd = -0x08a5c475eb66bd73;

// 5.1f
const int64_t kExpectedHashOfOnePointFive = 0x3ff8000000000000;

}  // namespace

TEST(IdentifiabilityMetricBuilderTest, SetChar) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, 'A')
                  .TakeEntry(),
              FirstMetricIs(INT64_C(65)));
}

TEST(IdentifiabilityMetricBuilderTest, SetCharArray) {
  IdentifiableToken sample(kAbcd);
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, sample)
                  .TakeEntry(),
              FirstMetricIs(kExpectedHashOfAbcd));
}

TEST(IdentifiabilityMetricBuilderTest, SetStringPiece) {
  // StringPiece() needs an explicit constructor invocation.
  EXPECT_THAT(
      IdentifiabilityMetricBuilder(base::UkmSourceId{})
          .Set(kTestSurface, IdentifiableToken(base::StringPiece(kAbcd)))
          .TakeEntry(),
      FirstMetricIs(kExpectedHashOfAbcd));
}

TEST(IdentifiabilityMetricBuilderTest, SetStdString) {
  IdentifiableToken sample((std::string(kAbcd)));
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, sample)
                  .TakeEntry(),
              FirstMetricIs(kExpectedHashOfAbcd));
}

TEST(IdentifiabilityMetricBuilderTest, SetInt) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, -5)
                  .TakeEntry(),
              FirstMetricIs(INT64_C(-5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetIntRef) {
  int x = -5;
  int& xref = x;
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, xref)
                  .TakeEntry(),
              FirstMetricIs(INT64_C(-5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetIntConstRef) {
  int x = -5;
  const int& xref = x;
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, xref)
                  .TakeEntry(),
              FirstMetricIs(INT64_C(-5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetUnsigned) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, 5u)
                  .TakeEntry(),
              FirstMetricIs(INT64_C(5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetUint64) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, UINT64_C(5))
                  .TakeEntry(),
              FirstMetricIs(INT64_C(5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetBigUnsignedInt) {
  // Slightly different in that this value cannot be converted into the sample
  // type without loss. Hence it is digested as raw bytes.
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, std::numeric_limits<uint64_t>::max())
                  .TakeEntry(),
              FirstMetricIs(INT64_C(-1)));
}

TEST(IdentifiabilityMetricBuilderTest, SetFloat) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, 1.5f)
                  .TakeEntry(),
              FirstMetricIs(kExpectedHashOfOnePointFive));
}

TEST(IdentifiabilityMetricBuilderTest, SetDouble) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, 1.5l)
                  .TakeEntry(),
              FirstMetricIs(kExpectedHashOfOnePointFive));
}

TEST(IdentifiabilityMetricBuilderTest, SetEnum) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, Never::kUp)
                  .TakeEntry(),
              FirstMetricIs(INT64_C(3)));
}

TEST(IdentifiabilityMetricBuilderTest, SetParameterPack) {
  EXPECT_THAT(IdentifiabilityMetricBuilder(base::UkmSourceId{})
                  .Set(kTestSurface, IdentifiableToken(1, 2, 3.0, 4, 'a'))
                  .TakeEntry(),
              FirstMetricIs(INT64_C(0x672cf4c107b5b22)));
}

}  // namespace blink
