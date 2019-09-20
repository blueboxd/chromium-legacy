// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_unittest_utils.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

// Tests that BlendForMinContrast(), with the default args, produces a transform
// that blends its input color to produce readable contrast against the
// specified background color.
TEST(ColorTransformTest, BlendForMinContrast) {
  const ColorTransform transform = BlendForMinContrast(kColorTest0);
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, kBackground}}});
  const auto verify_contrast = [&](SkColor input) {
    EXPECT_GE(
        color_utils::GetContrastRatio(transform.Run(input, mixer), kBackground),
        color_utils::kMinimumReadableContrastRatio);
  };
  verify_contrast(SK_ColorBLACK);
  verify_contrast(SK_ColorWHITE);
  verify_contrast(SK_ColorRED);
}

// Tests that BlendForMinContrast() supports optional args, which can be used to
// blend toward a specific foreground color and with a specific minimum contrast
// ratio.
TEST(ColorTransformTest, BlendForMinContrastOptionalArgs) {
  constexpr float kMinContrast = 6.0f;
  const ColorTransform transform =
      BlendForMinContrast(kColorTest0, kColorTest1, kMinContrast);
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer.AddSet(
      {kColorSetTest0,
       {{kColorTest0, kBackground}, {kColorTest1, gfx::kGoogleBlue900}}});
  const auto verify_contrast = [&](SkColor input) {
    EXPECT_GE(
        color_utils::GetContrastRatio(transform.Run(input, mixer), kBackground),
        kMinContrast);
  };
  verify_contrast(SK_ColorBLACK);
  verify_contrast(SK_ColorWHITE);
  verify_contrast(gfx::kGoogleBlue500);
}

// Tests that BlendTowardMaxContrast() produces a transform that blends its
// input color towards the color with max contrast.
TEST(ColorTransformTest, BlendTowardMaxContrast) {
  constexpr SkAlpha kAlpha = 0x20;
  const ColorTransform transform = BlendTowardMaxContrast(kAlpha);
  const auto verify_blend = [&](SkColor input) {
    const SkColor target = color_utils::GetColorWithMaxContrast(input);
    EXPECT_LT(color_utils::GetContrastRatio(transform.Run(input, ColorMixer()),
                                            target),
              color_utils::GetContrastRatio(input, target));
  };
  verify_blend(SK_ColorBLACK);
  verify_blend(SK_ColorWHITE);
  verify_blend(SK_ColorRED);
}

// Tests that DeriveDefaultIconColor() produces a transform that changes its
// input color.
TEST(ColorTransformTest, DeriveDefaultIconColor) {
  const ColorTransform transform = DeriveDefaultIconColor();
  const auto verify_derive = [&](SkColor input) {
    EXPECT_NE(input, transform.Run(input, ColorMixer()));
  };
  verify_derive(SK_ColorBLACK);
  verify_derive(SK_ColorWHITE);
  verify_derive(SK_ColorRED);
}

// Tests that FromColor() produces a transform that ignores the input color and
// always outputs a specified SkColor.
TEST(ColorTransformTest, FromColor) {
  constexpr SkColor kOutput = SK_ColorGREEN;
  const ColorTransform transform = FromColor(kOutput);
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(kOutput, transform.Run(input, ColorMixer()));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}

// Tests that FromOriginalColorFromSet() produces a transform that ignores the
// input color and always outputs a specified color from a specified color set.
TEST(ColorTransformTest, FromOriginalColorFromSet) {
  const ColorTransform transform =
      FromOriginalColorFromSet(kColorTest0, kColorSetTest0);
  constexpr SkColor kSet0Test0Color = SK_ColorGREEN;
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, kSet0Test0Color}}});
  mixer.AddSet({kColorSetTest1, {{kColorTest0, SK_ColorRED}}});
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(kSet0Test0Color, transform.Run(input, mixer));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}

// Tests that FromInputColor() produces a transform that ignores
// the input color and always outputs a specified color.
TEST(ColorTransformTest, FromInputColor) {
  const ColorTransform transform = FromInputColor(kColorTest0);
  constexpr SkColor kTest0Color = SK_ColorGREEN;
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0,
                {{kColorTest0, kTest0Color}, {kColorTest1, SK_ColorRED}}});
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(kTest0Color, transform.Run(input, mixer));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}  // namespace

// Tests that GetColorWithMaxContrast transforms white to the darkest color.
TEST(ColorTransformTest, GetColorWithMaxContrast) {
  const ColorTransform transform = GetColorWithMaxContrast();
  constexpr SkColor kNewDarkestColor = gfx::kGoogleGrey500;
  const SkColor default_darkest_color =
      color_utils::SetDarkestColorForTesting(kNewDarkestColor);
  constexpr SkColor kLightestColor = SK_ColorWHITE;
  EXPECT_EQ(kNewDarkestColor, transform.Run(kLightestColor, ColorMixer()));
  color_utils::SetDarkestColorForTesting(default_darkest_color);
  EXPECT_EQ(default_darkest_color, transform.Run(kLightestColor, ColorMixer()));
}

}  // namespace
}  // namespace ui
