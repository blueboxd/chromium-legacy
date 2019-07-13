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
    ColorInfo(0, SkColorSetRGB(192, 230, 181), "Pistachio"),
    ColorInfo(1, SkColorSetRGB(158, 228, 233), "Seafoam"),
    ColorInfo(2, SkColorSetRGB(219, 237, 254), "Mist"),
    ColorInfo(3, SkColorSetRGB(255, 174, 189), "Flamingo"),
    ColorInfo(4, SkColorSetRGB(221, 179, 197), "Wine"),
    ColorInfo(5, SkColorSetRGB(210, 175, 248), "Lavender"),
    ColorInfo(6, SkColorSetRGB(217, 213, 213), "Elephant"),
    ColorInfo(7, SkColorSetRGB(246, 97, 12), "Sunset"),
    ColorInfo(8, SkColorSetRGB(247, 194, 12), "Banana"),
    ColorInfo(9, SkColorSetRGB(109, 180, 87), "Kiwi"),
    ColorInfo(10, SkColorSetRGB(25, 157, 169), "Robins Egg"),
    ColorInfo(11, SkColorSetRGB(93, 147, 228), "Blue"),
    ColorInfo(12, SkColorSetRGB(23, 94, 26), "Pine"),
    ColorInfo(13, SkColorSetRGB(10, 110, 119), "Oceanic"),
    ColorInfo(14, SkColorSetRGB(126, 24, 39), "Bordeaux"),
    ColorInfo(15, SkColorSetRGB(91, 55, 137), "Eggplant"),
    ColorInfo(16, SkColorSetRGB(45, 65, 109), "Navy"),
    ColorInfo(17, SkColorSetRGB(33, 33, 33), "Midnight"),
    ColorInfo(18, SkColorSetRGB(189, 22, 82), "Raspberry")};

}  // namespace chrome_colors

#endif  // CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_
