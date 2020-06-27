// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_caption_button.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme_base.h"

namespace {

constexpr SkColor kBackgroundColors[] = {
    SK_ColorBLACK,  SK_ColorDKGRAY,
    SK_ColorGRAY,   SK_ColorLTGRAY,
    SK_ColorWHITE,  SK_ColorRED,
    SK_ColorYELLOW, SK_ColorCYAN,
    SK_ColorBLUE,   SkColorSetRGB(230, 138, 90),
};

// A test theme that always returns a fixed color.
class TestNativeTheme : public ui::NativeThemeBase {
 public:
  TestNativeTheme() = default;
  TestNativeTheme(const TestNativeTheme&) = delete;
  TestNativeTheme& operator=(const TestNativeTheme&) = delete;

  // NativeThemeBase:
  SkColor GetSystemColor(ColorId color_id,
                         ColorScheme color_scheme) const override {
    return color_scheme == ui::NativeTheme::ColorScheme::kDark
               ? gfx::kGoogleGrey200
               : gfx::kGoogleGrey700;
  }
};

}  // namespace

TEST(FrameCaptionButtonTest, ThemedColorContrast) {
  TestNativeTheme theme;
  for (SkColor background_color : kBackgroundColors) {
    SkColor button_color =
        theme.GetFrameCaptionButtonForegroundColor(background_color);
    EXPECT_GE(color_utils::GetContrastRatio(button_color, background_color), 3);
  }
}
