// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "chrome/common/search/instant_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace internal {

// Defined in searchbox_extension.cc
bool IsNtpBackgroundDark(SkColor ntp_text);
SkColor CalculateIconColor(SkColor bg_color);
SkColor GetIconColor(const ThemeBackgroundInfo& theme_info);

TEST(SearchboxExtensionTest, TestIsNtpBackgroundDark) {
  // Dark font means light background.
  EXPECT_FALSE(IsNtpBackgroundDark(SK_ColorBLACK));

  // Light font means dark background.
  EXPECT_TRUE(IsNtpBackgroundDark(SK_ColorWHITE));

  // Light but close to mid point color text implies dark background.
  EXPECT_TRUE(IsNtpBackgroundDark(SkColorSetARGB(255, 30, 144, 255)));
}

TEST(SearchboxExtensionTest, TestCalculateIconColor) {
  // White icon for black background.
  EXPECT_EQ(SK_ColorWHITE, CalculateIconColor(SK_ColorBLACK));

  // Lighter icon for too dark colors.
  SkColor dark_background = SkColorSetARGB(255, 50, 0, 50);
  EXPECT_LT(
      color_utils::GetRelativeLuminance(dark_background),
      color_utils::GetRelativeLuminance(CalculateIconColor(dark_background)));

  // Darker icon for light backgrounds.
  EXPECT_GT(
      color_utils::GetRelativeLuminance(SK_ColorWHITE),
      color_utils::GetRelativeLuminance(CalculateIconColor(SK_ColorWHITE)));

  SkColor light_background = SkColorSetARGB(255, 100, 0, 100);
  EXPECT_GT(
      color_utils::GetRelativeLuminance(light_background),
      color_utils::GetRelativeLuminance(CalculateIconColor(light_background)));
}

TEST(SearchboxExtensionTest, TestGetIconColor) {
  constexpr SkColor kLightIconColor = gfx::kGoogleGrey100;
  constexpr SkColor kDarkIconColor = gfx::kGoogleGrey900;

  ThemeBackgroundInfo theme_info;
  theme_info.using_default_theme = true;
  theme_info.using_dark_mode = false;
  theme_info.background_color = SK_ColorRED;

  // // Default theme in light mode.
  EXPECT_EQ(kLightIconColor, GetIconColor(theme_info));

  // // Default theme in dark mode.
  theme_info.using_dark_mode = true;
  EXPECT_EQ(kDarkIconColor, GetIconColor(theme_info));

  // Default theme with custom background, in dark mode.
  theme_info.custom_background_url = GURL("https://www.foo.com");
  EXPECT_EQ(kLightIconColor, GetIconColor(theme_info));

  // Default theme with custom background.
  theme_info.using_dark_mode = false;
  EXPECT_EQ(kLightIconColor, GetIconColor(theme_info));

  // Theme with image.
  theme_info.using_default_theme = false;
  theme_info.custom_background_url = GURL();
  theme_info.has_theme_image = true;
  EXPECT_EQ(kLightIconColor, GetIconColor(theme_info));

  // Theme with image in dark mode.
  theme_info.using_dark_mode = true;
  EXPECT_EQ(kLightIconColor, GetIconColor(theme_info));

  SkColor red_icon_color = CalculateIconColor(SK_ColorRED);

  // Theme with no image, in dark mode.
  theme_info.has_theme_image = false;
  EXPECT_EQ(red_icon_color, GetIconColor(theme_info));

  // Theme with no image.
  theme_info.using_dark_mode = false;
  EXPECT_EQ(red_icon_color, GetIconColor(theme_info));
}

}  // namespace internal
