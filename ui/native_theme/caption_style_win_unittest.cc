// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ui {

// Test to ensure closed caption styling from system settings is used on
// Windows 10.
TEST(CaptionStyleWinTest, TestWinCaptionStyle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kSystemCaptionStyle);

  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    base::win::ScopedCOMInitializer com_initializer;
    ASSERT_TRUE(com_initializer.Succeeded());

    base::Optional<ui::CaptionStyle> caption_style =
        ui::CaptionStyle::FromSystemSettings();
    // Other caption style properties can be empty and shouldn't be checked.
    ASSERT_TRUE(caption_style.has_value());
    EXPECT_TRUE(!caption_style->background_color.empty());
    EXPECT_TRUE(!caption_style->text_color.empty());
    EXPECT_TRUE(!caption_style->font_variant.empty());
  }
}

}  // namespace ui
