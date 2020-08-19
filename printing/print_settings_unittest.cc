// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintSettingsTest, IsColorModelSelected) {
  for (int model = static_cast<int>(mojom::ColorModel::kUnknownColorModel) + 1;
       model <= static_cast<int>(mojom::ColorModel::kColorModelLast); ++model)
    EXPECT_TRUE(IsColorModelSelected(IsColorModelSelected(model).has_value()));
}

TEST(PrintSettingsDeathTest, IsColorModelSelectedEdges) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DCHECK_DEATH(IsColorModelSelected(
      static_cast<int>(mojom::ColorModel::kUnknownColorModel)));
  EXPECT_DCHECK_DEATH(IsColorModelSelected(
      static_cast<int>(mojom::ColorModel::kUnknownColorModel) - 1));
  EXPECT_DCHECK_DEATH(IsColorModelSelected(
      static_cast<int>(mojom::ColorModel::kColorModelLast) + 1));
}

#if defined(USE_CUPS)
TEST(PrintSettingsTest, GetColorModelForMode) {
  std::string color_setting_name;
  std::string color_value;
  for (int model = static_cast<int>(mojom::ColorModel::kUnknownColorModel);
       model <= static_cast<int>(mojom::ColorModel::kColorModelLast); ++model) {
    GetColorModelForMode(model, &color_setting_name, &color_value);
    EXPECT_FALSE(color_setting_name.empty());
    EXPECT_FALSE(color_value.empty());
    color_setting_name.clear();
    color_value.clear();
  }
}

TEST(PrintSettingsDeathTest, GetColorModelForModeEdges) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  std::string color_setting_name;
  std::string color_value;
  EXPECT_DCHECK_DEATH(GetColorModelForMode(
      static_cast<int>(mojom::ColorModel::kUnknownColorModel) - 1,
      &color_setting_name, &color_value));
  EXPECT_DCHECK_DEATH(GetColorModelForMode(
      static_cast<int>(mojom::ColorModel::kColorModelLast) + 1,
      &color_setting_name, &color_value));
}

#if defined(OS_MAC) || defined(OS_CHROMEOS)
TEST(PrintSettingsTest, GetIppColorModelForMode) {
  for (int model = static_cast<int>(mojom::ColorModel::kUnknownColorModel);
       model <= static_cast<int>(mojom::ColorModel::kColorModelLast); ++model)
    EXPECT_FALSE(GetIppColorModelForMode(model).empty());
}

TEST(PrintSettingsDeathTest, GetIppColorModelForModeEdges) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DCHECK_DEATH(GetIppColorModelForMode(
      static_cast<int>(mojom::ColorModel::kUnknownColorModel) - 1));
  EXPECT_DCHECK_DEATH(GetIppColorModelForMode(
      static_cast<int>(mojom::ColorModel::kColorModelLast) + 1));
}
#endif  // defined(OS_MAC) || defined(OS_CHROMEOS)
#endif  // defined(USE_CUPS)

}  // namespace printing
