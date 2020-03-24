// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <ostream>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme_color_id.h"

namespace ui {

namespace {

constexpr const char* kColorIdStringName[] = {
#define OP(enum_name) #enum_name
    NATIVE_THEME_COLOR_IDS
#undef OP
};

struct PrintableSkColor {
  bool operator==(const PrintableSkColor& other) const {
    return color == other.color;
  }

  bool operator!=(const PrintableSkColor& other) const {
    return !operator==(other);
  }

  const SkColor color;
};

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color) {
  SkColor color = printable_color.color;
  return os << base::StringPrintf("SkColorARGB(0x%02x, 0x%02x, 0x%02x, 0x%02x)",
                                  SkColorGetA(color), SkColorGetR(color),
                                  SkColorGetG(color), SkColorGetB(color));
}

class NativeThemeRedirectedEquivalenceTest
    : public testing::TestWithParam<NativeTheme::ColorId> {
 public:
  NativeThemeRedirectedEquivalenceTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<NativeTheme::ColorId> param_info) {
    NativeTheme::ColorId color_id = param_info.param;
    if (color_id >= NativeTheme::ColorId::kColorId_NumColors) {
      ADD_FAILURE() << "Invalid color value " << color_id;
      return "Invalid";
    }
    return kColorIdStringName[color_id];
  }
};

}  // namespace

TEST_P(NativeThemeRedirectedEquivalenceTest, NativeUiGetSystemColor) {
  // Verifies that colors with and without the Color Provider are the same.
  NativeTheme* native_theme = NativeTheme::GetInstanceForNativeUi();
  NativeTheme::ColorId color_id = GetParam();

  PrintableSkColor original{native_theme->GetSystemColor(color_id)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kColorProviderRedirection);
  PrintableSkColor redirected{native_theme->GetSystemColor(color_id)};

  EXPECT_EQ(original, redirected);
}

#define OP(enum_name) NativeTheme::ColorId::enum_name
INSTANTIATE_TEST_SUITE_P(
    ,
    NativeThemeRedirectedEquivalenceTest,
    ::testing::Values(NATIVE_THEME_COLOR_IDS),
    NativeThemeRedirectedEquivalenceTest::ParamInfoToString);
#undef OP

}  // namespace ui
