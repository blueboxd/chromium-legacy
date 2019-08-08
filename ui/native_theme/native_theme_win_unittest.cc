// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

using Scheme = ui::NativeTheme::PreferredColorScheme;
using SystemThemeColor = ui::NativeTheme::SystemThemeColor;

class TestNativeThemeWin : public NativeThemeWin {
 public:
  TestNativeThemeWin() {}
  ~TestNativeThemeWin() override {}

  // NativeTheme:
  bool UsesHighContrastColors() const override { return high_contrast_; }
  bool ShouldUseDarkColors() const override { return dark_mode_; }

  void SetDarkMode(bool dark_mode) { dark_mode_ = dark_mode; }
  void SetUsesHighContrastColors(bool high_contrast) {
    high_contrast_ = high_contrast;
  }
  void SetSystemColor(SystemThemeColor system_color, SkColor color) {
    system_colors_[system_color] = color;
  }

 private:
  bool dark_mode_ = false;
  bool high_contrast_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestNativeThemeWin);
};

TEST(NativeThemeWinTest, CalculatePreferredColorScheme) {
  TestNativeThemeWin theme;

  theme.SetUsesHighContrastColors(false);
  theme.SetDarkMode(true);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), Scheme::kDark);

  theme.SetDarkMode(false);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), Scheme::kLight);

  theme.SetUsesHighContrastColors(true);
  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorBLACK);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorWHITE);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), Scheme::kDark);

  theme.SetSystemColor(SystemThemeColor::kWindow, SK_ColorWHITE);
  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLACK);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), Scheme::kLight);

  theme.SetSystemColor(SystemThemeColor::kWindowText, SK_ColorBLUE);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), Scheme::kNoPreference);

  theme.SetUsesHighContrastColors(false);
  ASSERT_EQ(theme.CalculatePreferredColorScheme(), Scheme::kLight);
}

}  // namespace ui
