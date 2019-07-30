// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_
#define CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_

#include <stdint.h>

#include "third_party/skia/include/core/SkColor.h"

namespace chrome_colors {

struct ColorInfo {
  constexpr ColorInfo(int id, SkColor color, const char* label)
      : ColorInfo(id, color, label, nullptr) {}
  constexpr ColorInfo(int id,
                      SkColor color,
                      const char* label,
                      const char* icon_data)
      : id(id), color(color), label(label), icon_data(icon_data) {}
  int id;
  SkColor color;
  const char* label;
  const char* icon_data;
};

// TODO(gayane): Add colors selected by UX.
// List of preselected colors to show in Chrome Colors menu. This array should
// always be in sync with ChromeColorsInfo in enums.xml.
constexpr ColorInfo kSelectedColorsInfo[] = {
    // 0  - reserved for any color not in this set.
    ColorInfo(1, SkColorSetRGB(239, 235, 233), "Elephant"),
    ColorInfo(2, SkColorSetRGB(120, 127, 145), "Light grey"),
    ColorInfo(3, SkColorSetRGB(55, 71, 79), "Midnight"),
    ColorInfo(4, SkColorSetRGB(0, 0, 0), "Black"),
    ColorInfo(5, SkColorSetRGB(252, 219, 201), "Beige/White"),
    ColorInfo(6, SkColorSetRGB(255, 249, 228), "Yellow/White"),
    ColorInfo(7, SkColorSetRGB(203, 233, 191), "Green/White"),
    ColorInfo(8, SkColorSetRGB(221, 244, 249), "Light Teal/White"),
    ColorInfo(9, SkColorSetRGB(233, 212, 255), "Light Purple/White"),
    ColorInfo(10, SkColorSetRGB(249, 226, 237), "Pink/White"),
    ColorInfo(11, SkColorSetRGB(227, 171, 154), "Beige"),
    ColorInfo(12, SkColorSetRGB(255, 171, 64), "Orange"),
    ColorInfo(13, SkColorSetRGB(67, 160, 71), "Light Green"),
    ColorInfo(14, SkColorSetRGB(25, 157, 169), "Light Teal"),
    ColorInfo(15, SkColorSetRGB(93, 147, 228), "Light Blue"),
    ColorInfo(16, SkColorSetRGB(255, 174, 189), "Pink"),
    ColorInfo(17, SkColorSetRGB(189, 22, 92), "Dark Pink/Red"),
    ColorInfo(18, SkColorSetRGB(183, 28, 28), "Dark Red/Orange"),
    ColorInfo(19, SkColorSetRGB(46, 125, 50), "Dark Green"),
    ColorInfo(20, SkColorSetRGB(0, 110, 120), "Dark Teal"),
    ColorInfo(21, SkColorSetRGB(21, 101, 192), "Dark Blue"),
    ColorInfo(22, SkColorSetRGB(91, 54, 137), "Dark Purple"),
};

}  // namespace chrome_colors

#endif  // CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_
