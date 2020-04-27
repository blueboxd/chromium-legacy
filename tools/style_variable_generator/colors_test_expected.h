// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_STYLE_VARIABLE_GENERATOR_COLORS_TEST_EXPECTED_H_
#define TOOLS_STYLE_VARIABLE_GENERATOR_COLORS_TEST_EXPECTED_H_

#include "ash/style/ash_color_provider.h"

namespace colors_test_expected {

using AshColorMode = ash::AshColorProvider::AshColorMode;

enum class ColorName {
  kGoogleGrey900,
  kCrosDefaultTextColor,
  kCrosToggleColor,
};

constexpr SkColor ResolveColor(ColorName color_name, AshColorMode color_mode) {
  switch (color_name) {
    case ColorName::kGoogleGrey900:
      return SkColorSetRGB(0x20, 0x21, 0x24);
    case ColorName::kCrosDefaultTextColor:
      if (color_mode == AshColorMode::kLight) {
        return ResolveColor(ColorName::kGoogleGrey900, color_mode);
      } else {
        return SkColorSetRGB(0xFF, 0xFF, 0xFF);
      }
    case ColorName::kCrosToggleColor:
      return SkColorSetA(ResolveColor(ColorName::kCrosDefaultTextColor, color_mode), 0x19);
  }
}

}  // namespace colors_test_expected
#endif  // TOOLS_STYLE_VARIABLE_GENERATOR_COLORS_TEST_EXPECTED_H_
