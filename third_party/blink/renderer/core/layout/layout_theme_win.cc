// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_win.h"

#include <windows.h>

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeWin::Create() {
  return base::AdoptRef(new LayoutThemeWin());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeWin::Create()));
  return *layout_theme;
}

Color LayoutThemeWin::SystemColor(CSSValueID css_value_id,
                                  WebColorScheme color_scheme) const {
  if (!RuntimeEnabledFeatures::UseWindowsSystemColorsEnabled()) {
    return LayoutThemeDefault::SystemColor(css_value_id, color_scheme);
  }

  int system_index;

  switch (css_value_id) {
    case CSSValueID::kButtonface:
      system_index = COLOR_BTNFACE;
      break;
    case CSSValueID::kButtontext:
      system_index = COLOR_BTNTEXT;
      break;
    case CSSValueID::kGraytext:
      system_index = COLOR_GRAYTEXT;
      break;
    case CSSValueID::kHighlight:
      system_index = COLOR_HIGHLIGHT;
      break;
    case CSSValueID::kHighlighttext:
      system_index = COLOR_HIGHLIGHTTEXT;
      break;
    case CSSValueID::kLinktext:
    case CSSValueID::kVisitedtext:
      system_index = COLOR_HOTLIGHT;
      break;
    case CSSValueID::kWindow:
      system_index = COLOR_WINDOW;
      break;
    case CSSValueID::kWindowtext:
      system_index = COLOR_WINDOWTEXT;
      break;
    default:
      return LayoutThemeDefault::SystemColor(css_value_id, color_scheme);
  }

  return SystemColorBySystemIndex(system_index);
}

Color LayoutThemeWin::SystemColorBySystemIndex(int system_index) {
  DWORD system_color = ::GetSysColor(system_index);
  return Color(GetRValue(system_color), GetGValue(system_color),
               GetBValue(system_color));
}

}  // namespace blink
