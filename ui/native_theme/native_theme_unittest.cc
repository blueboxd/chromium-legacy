// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <ostream>
#include <tuple>

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
    : public testing::TestWithParam<
          std::tuple<NativeTheme::ColorScheme, NativeTheme::ColorId>> {
 public:
  NativeThemeRedirectedEquivalenceTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<std::tuple<NativeTheme::ColorScheme,
                                          NativeTheme::ColorId>> param_info) {
    auto param_tuple = param_info.param;
    return ColorSchemeToString(std::get<0>(param_tuple)) + "_With_" +
           ColorIdToString(std::get<1>(param_tuple));
  }

 private:
  static std::string ColorSchemeToString(NativeTheme::ColorScheme scheme) {
    switch (scheme) {
      case NativeTheme::ColorScheme::kDefault:
        NOTREACHED()
            << "Cannot unit test kDefault as it depends on machine state.";
        return "InvalidColorScheme";
      case NativeTheme::ColorScheme::kLight:
        return "kLight";
      case NativeTheme::ColorScheme::kDark:
        return "kDark";
      case NativeTheme::ColorScheme::kPlatformHighContrast:
        return "kPlatformHighContrast";
    }
  }

  static std::string ColorIdToString(NativeTheme::ColorId id) {
    if (id >= NativeTheme::ColorId::kColorId_NumColors) {
      NOTREACHED() << "Invalid color value " << id;
      return "InvalidColorId";
    }
    return kColorIdStringName[id];
  }
};

}  // namespace

TEST_P(NativeThemeRedirectedEquivalenceTest, NativeUiGetSystemColor) {
  // Verifies that colors with and without the Color Provider are the same.
  NativeTheme* native_theme = NativeTheme::GetInstanceForNativeUi();
  auto param_tuple = GetParam();
  auto color_scheme = std::get<0>(param_tuple);
  auto color_id = std::get<1>(param_tuple);

  PrintableSkColor original{
      native_theme->GetSystemColor(color_id, color_scheme)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kColorProviderRedirection);
  PrintableSkColor redirected{
      native_theme->GetSystemColor(color_id, color_scheme)};

  EXPECT_EQ(original, redirected);
}

#define OP(enum_name) NativeTheme::ColorId::enum_name
INSTANTIATE_TEST_SUITE_P(
    ,
    NativeThemeRedirectedEquivalenceTest,
    ::testing::Combine(
        ::testing::Values(NativeTheme::ColorScheme::kLight,
                          NativeTheme::ColorScheme::kDark,
                          NativeTheme::ColorScheme::kPlatformHighContrast),
        ::testing::Values(NATIVE_THEME_COLOR_IDS)),
    NativeThemeRedirectedEquivalenceTest::ParamInfoToString);
#undef OP

}  // namespace ui
