// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include "base/optional.h"
#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace {

// TODO(gilmanmh): Add expectations for image inversion to each test.
TEST(DarkModeFilterTest, DoNotApplyFilterWhenDarkModeIsOff) {
  DarkModeFilter filter;

  DarkModeSettings settings;
  settings.mode = DarkMode::kOff;
  filter.UpdateSettings(settings);

  EXPECT_EQ(Color::kWhite, filter.InvertColorIfNeeded(Color::kWhite));
  EXPECT_EQ(Color::kBlack, filter.InvertColorIfNeeded(Color::kBlack));

  EXPECT_EQ(base::nullopt, filter.ApplyToFlagsIfNeeded(cc::PaintFlags()));

  EXPECT_FALSE(filter.ShouldInvertTextColor(Color::kWhite));
  EXPECT_FALSE(filter.ShouldInvertTextColor(Color::kBlack));
}

TEST(DarkModeFilterTest, ApplyDarkModeToColorsFlagsAndText) {
  DarkModeFilter filter;

  DarkModeSettings settings;
  settings.mode = DarkMode::kSimpleInvertForTesting;
  filter.UpdateSettings(settings);

  EXPECT_EQ(Color::kBlack, filter.InvertColorIfNeeded(Color::kWhite));
  EXPECT_EQ(Color::kWhite, filter.InvertColorIfNeeded(Color::kBlack));

  cc::PaintFlags flags;
  flags.setColor(SK_ColorWHITE);
  auto flags_or_nullopt = filter.ApplyToFlagsIfNeeded(flags);
  ASSERT_NE(flags_or_nullopt, base::nullopt);
  EXPECT_EQ(SK_ColorBLACK, flags_or_nullopt.value().getColor());

  EXPECT_TRUE(filter.ShouldInvertTextColor(Color::kWhite));
  EXPECT_TRUE(filter.ShouldInvertTextColor(Color::kBlack));
}

TEST(DarkModeFilterTest, ApplyFilterToDarkTextOnly) {
  DarkModeFilter filter;

  DarkModeSettings settings;
  settings.mode = DarkMode::kSimpleInvertForTesting;
  settings.text_brightness_threshold = 200;
  filter.UpdateSettings(settings);

  EXPECT_FALSE(filter.ShouldInvertTextColor(Color::kWhite));
  EXPECT_TRUE(filter.ShouldInvertTextColor(Color::kBlack));

  EXPECT_FALSE(filter.ShouldInvertTextColor(Color(
      settings.text_brightness_threshold, settings.text_brightness_threshold,
      settings.text_brightness_threshold)));

  EXPECT_TRUE(filter.ShouldInvertTextColor(
      Color(settings.text_brightness_threshold - 5,
            settings.text_brightness_threshold - 5,
            settings.text_brightness_threshold - 5)));
  EXPECT_FALSE(filter.ShouldInvertTextColor(
      Color(settings.text_brightness_threshold + 5,
            settings.text_brightness_threshold + 5,
            settings.text_brightness_threshold + 5)));
}

}  // namespace
}  // namespace blink
