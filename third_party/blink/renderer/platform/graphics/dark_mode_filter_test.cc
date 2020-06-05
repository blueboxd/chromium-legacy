// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "base/optional.h"
#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

TEST(DarkModeFilterTest, DoNotApplyFilterWhenDarkModeIsOff) {
  DarkModeFilter filter;

  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kOff;
  filter.UpdateSettings(settings);

  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorBLACK, DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(base::nullopt,
            filter.ApplyToFlagsIfNeeded(
                cc::PaintFlags(), DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(nullptr, filter.GetImageFilterForTesting());
}

TEST(DarkModeFilterTest, ApplyDarkModeToColorsAndFlags) {
  DarkModeFilter filter;

  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  filter.UpdateSettings(settings);

  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(
                SK_ColorBLACK, DarkModeFilter::ElementRole::kBackground));

  EXPECT_EQ(SK_ColorWHITE,
            filter.InvertColorIfNeeded(SK_ColorWHITE,
                                       DarkModeFilter::ElementRole::kSVG));
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(SK_ColorBLACK,
                                       DarkModeFilter::ElementRole::kSVG));

  cc::PaintFlags flags;
  flags.setColor(SK_ColorWHITE);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(
      flags, DarkModeFilter::ElementRole::kBackground);
  ASSERT_NE(flags_or_nullopt, base::nullopt);
  EXPECT_EQ(SK_ColorBLACK, flags_or_nullopt.value().getColor());

  EXPECT_NE(nullptr, filter.GetImageFilterForTesting());
}

TEST(DarkModeFilterTest, InvertedColorCacheSize) {
  DarkModeFilter filter;
  DarkModeSettings settings;

  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  filter.UpdateSettings(settings);
  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  // Should get cached value.
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());

  // On changing DarkModeSettings, cache should be reset.
  settings.mode = DarkModeInversionAlgorithm::kInvertLightness;
  filter.UpdateSettings(settings);
  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
}

TEST(DarkModeFilterTest, InvertedColorCacheZeroMaxKeys) {
  DarkModeFilter filter;
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  filter.UpdateSettings(settings);

  EXPECT_EQ(0u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(1u, filter.GetInvertedColorCacheSizeForTesting());
  EXPECT_EQ(SK_ColorTRANSPARENT,
            filter.InvertColorIfNeeded(
                SK_ColorTRANSPARENT, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());

  // Results returned from cache.
  EXPECT_EQ(SK_ColorBLACK,
            filter.InvertColorIfNeeded(
                SK_ColorWHITE, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            filter.InvertColorIfNeeded(
                SK_ColorTRANSPARENT, DarkModeFilter::ElementRole::kBackground));
  EXPECT_EQ(2u, filter.GetInvertedColorCacheSizeForTesting());
}

}  // namespace
}  // namespace blink
