// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "ui/color/color_id.h"

// TODO(pkasting): Add the rest of the colors.

// clang-format off
#define CHROME_COLOR_IDS \
  /* Omnibox output colors. */ \
  E(kColorOmniboxBackground, kChromeColorsStart), \
  E(kColorOmniboxBackgroundHovered), \
  E(kColorOmniboxBubbleOutline), \
  E(kColorOmniboxBubbleOutlineExperimentalKeywordMode), \
  E(kColorOmniboxResultsBackground), \
  E(kColorOmniboxResultsBackgroundHovered), \
  E(kColorOmniboxResultsBackgroundSelected), \
  E(kColorOmniboxResultsIcon), \
  E(kColorOmniboxResultsIconSelected), \
  E(kColorOmniboxResultsTextDimmed), \
  E(kColorOmniboxResultsTextDimmedSelected), \
  E(kColorOmniboxResultsTextSelected), \
  E(kColorOmniboxResultsUrl), \
  E(kColorOmniboxResultsUrlSelected), \
  E(kColorOmniboxSecurityChipDangerous), \
  E(kColorOmniboxSecurityChipDefault), \
  E(kColorOmniboxSecurityChipSecure), \
  E(kColorOmniboxSelectedKeyword), \
  E(kColorOmniboxText), \
  E(kColorOmniboxTextDimmed), \
  \
  E(kColorToolbar)
// clang-format on

#include "ui/color/color_id_macros.inc"

enum ChromeColorIds : ui::ColorId {
  kChromeColorsStart = ui::kUiColorsEnd,

  CHROME_COLOR_IDS,

  kChromeColorsEnd,
};

#include "ui/color/color_id_macros.inc"

static_assert(ui::ColorId{kChromeColorsEnd} <= ui::ColorId{ui::kUiColorsLast},
              "Embedder colors must not exceed allowed space");

enum ChromeColorSetIds : ui::ColorSetId {
  kColorSetCustomTheme = ui::kUiColorSetsEnd,

  kChromeColorSetsEnd,
};

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
