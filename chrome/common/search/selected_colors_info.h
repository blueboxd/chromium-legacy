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
// List of preselected colors to show in Chrome Colors menu.
constexpr ColorInfo kSelectedColorsInfo[] = {
    ColorInfo(0, SkColorSetRGB(120, 0, 120), "purple"),
    ColorInfo(1, SkColorSetRGB(0, 100, 100), "teal"),
    ColorInfo(2, SkColorSetRGB(200, 50, 100), "pretty"),
    ColorInfo(3, SkColorSetRGB(70, 50, 170), "bluish")};

}  // namespace chrome_colors

#endif  // CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_
