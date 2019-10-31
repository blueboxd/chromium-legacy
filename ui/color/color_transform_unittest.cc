// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_unittest_utils.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

// Tests that BlendForMinContrast(), with the default args, produces a transform
// that blends its foreground color to produce readable contrast against its
// background color.
TEST(ColorTransformTest, BlendForMinContrast) {
  const ColorTransform transform =
      BlendForMinContrast(FromTransformInput(), FromInputColor(kColorTest0));
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
      BlendForMinContrast(FromTransformInput(), FromInputColor(kColorTest0),
                          FromInputColor(kColorTest1), kMinContrast);
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
  const ColorTransform transform =
      BlendTowardMaxContrast(FromTransformInput(), kAlpha);
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

// Tests that ContrastInvert() produces a transform that outputs a color with at
// least as much contrast, but against the opposite endpoint.
TEST(ColorTransformTest, ContrastInvert) {
  const ColorTransform transform = ContrastInvert(FromTransformInput());
  const auto verify_invert = [&](SkColor input) {
    const SkColor far_endpoint = color_utils::GetColorWithMaxContrast(input);
    const SkColor near_endpoint =
        color_utils::GetColorWithMaxContrast(far_endpoint);
    EXPECT_GE(color_utils::GetContrastRatio(transform.Run(input, ColorMixer()),
                                            near_endpoint),
              color_utils::GetContrastRatio(input, far_endpoint));
  };
  verify_invert(gfx::kGoogleGrey900);
  verify_invert(SK_ColorWHITE);
  verify_invert(SK_ColorRED);
  verify_invert(gfx::kGoogleBlue500);
}

// Tests that DeriveDefaultIconColor() produces a transform that changes its
// input color.
TEST(ColorTransformTest, DeriveDefaultIconColor) {
  const ColorTransform transform = DeriveDefaultIconColor(FromTransformInput());
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
// the input color and always outputs a specified input color.
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

// Tests that FromResultColor() produces a transform that ignores
// the input color and always outputs a specified result color.
TEST(ColorTransformTest, FromResultColor) {
  const ColorTransform transform = FromResultColor(kColorTest0);
  constexpr SkColor kTest1Color = SK_ColorRED;
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0,
                {{kColorTest0, SK_ColorGREEN}, {kColorTest1, kTest1Color}}});
  mixer.AddRecipe(kColorTest0).AddTransform(FromInputColor(kColorTest1));
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(kTest1Color, transform.Run(input, mixer));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}  // namespace

// Tests that FromTransformInput() returns its input color unmodified.
TEST(ColorTransformTest, FromTransformInput) {
  const ColorTransform transform = FromTransformInput();
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(input, transform.Run(input, ColorMixer()));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}  // namespace

// Tests that GetColorWithMaxContrast() produces a transform that changes white
// to the darkest color.
TEST(ColorTransformTest, GetColorWithMaxContrast) {
  const ColorTransform transform =
      GetColorWithMaxContrast(FromTransformInput());
  constexpr SkColor kNewDarkestColor = gfx::kGoogleGrey500;
  const SkColor default_darkest_color =
      color_utils::SetDarkestColorForTesting(kNewDarkestColor);
  constexpr SkColor kLightestColor = SK_ColorWHITE;
  EXPECT_EQ(kNewDarkestColor, transform.Run(kLightestColor, ColorMixer()));
  color_utils::SetDarkestColorForTesting(default_darkest_color);
  EXPECT_EQ(default_darkest_color, transform.Run(kLightestColor, ColorMixer()));
}

// Tests that GetResultingPaintColor() produces a transform that composites
// opaquely.
TEST(ColorTransformTest, GetResultingPaintColor) {
  const ColorTransform transform =
      GetResultingPaintColor(FromTransformInput(), FromInputColor(kColorTest0));
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, kBackground}}});
  EXPECT_EQ(SK_ColorBLACK, transform.Run(SK_ColorBLACK, mixer));
  EXPECT_EQ(kBackground, transform.Run(SK_ColorTRANSPARENT, mixer));
  EXPECT_EQ(color_utils::AlphaBlend(SK_ColorBLACK, kBackground, SkAlpha{0x80}),
            transform.Run(SkColorSetA(SK_ColorBLACK, 0x80), mixer));
}

// Tests that SelectBasedOnDarkInput() produces a transform that toggles between
// inputs based on whether the input color is dark.
TEST(ColorTransformTest, SelectBasedOnDarkInput) {
  constexpr SkColor kDarkOutput = SK_ColorGREEN;
  constexpr SkColor kLightOutput = SK_ColorRED;
  const ColorTransform transform = SelectBasedOnDarkInput(
      FromTransformInput(), FromColor(kDarkOutput), FromColor(kLightOutput));
  EXPECT_EQ(kDarkOutput, transform.Run(SK_ColorBLACK, ColorMixer()));
  EXPECT_EQ(kLightOutput, transform.Run(SK_ColorWHITE, ColorMixer()));
  EXPECT_EQ(kDarkOutput, transform.Run(SK_ColorBLUE, ColorMixer()));
  EXPECT_EQ(kLightOutput, transform.Run(SK_ColorRED, ColorMixer()));
}

}  // namespace
}  // namespace ui
