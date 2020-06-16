// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"

#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

StyleColor::StyleColor() : color_keyword_(CSSValueID::kCurrentcolor) {}

StyleColor::StyleColor(Color color)
    : color_(color), color_keyword_(CSSValueID::kInvalid) {}

StyleColor::StyleColor(CSSValueID keyword) : color_keyword_(keyword) {}

StyleColor StyleColor::CurrentColor() {
  return StyleColor();
}

bool StyleColor::IsCurrentColor() const {
  return color_keyword_ == CSSValueID::kCurrentcolor;
}

Color StyleColor::GetColor() const {
  DCHECK(!IsCurrentColor());
  return color_;
}

Color StyleColor::Resolve(Color current_color) const {
  return IsCurrentColor() ? current_color : color_;
}

bool StyleColor::HasAlpha() const {
  return !IsCurrentColor() && color_.HasAlpha();
}

Color StyleColor::ColorFromKeyword(CSSValueID keyword,
                                   WebColorScheme color_scheme) {
  if (const char* value_name = getValueName(keyword)) {
    if (const NamedColor* named_color =
            FindColor(value_name, static_cast<wtf_size_t>(strlen(value_name))))
      return Color(named_color->argb_value);
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
}

bool StyleColor::IsColorKeyword(CSSValueID id) {
  // Named colors and color keywords:
  //
  // <named-color>
  //   'aqua', 'black', 'blue', ..., 'yellow' (CSS3: "basic color keywords")
  //   'aliceblue', ..., 'yellowgreen'        (CSS3: "extended color keywords")
  //   'transparent'
  //
  // 'currentcolor'
  //
  // <deprecated-system-color>
  //   'ActiveBorder', ..., 'WindowText'
  //
  // WebKit proprietary/internal:
  //   '-webkit-link'
  //   '-webkit-activelink'
  //   '-internal-active-list-box-selection'
  //   '-internal-active-list-box-selection-text'
  //   '-internal-inactive-list-box-selection'
  //   '-internal-inactive-list-box-selection-text'
  //   '-webkit-focus-ring-color'
  //   '-internal-quirk-inherit'
  //
  return (id >= CSSValueID::kAqua && id <= CSSValueID::kInternalQuirkInherit) ||
         (id >= CSSValueID::kAliceblue && id <= CSSValueID::kYellowgreen) ||
         id == CSSValueID::kMenu;
}

bool StyleColor::IsSystemColor(CSSValueID id) {
  return (id >= CSSValueID::kActiveborder && id <= CSSValueID::kWindowtext) ||
         id == CSSValueID::kMenu;
}

bool operator==(const StyleColor& a, const StyleColor& b) {
  if (a.IsCurrentColor() || b.IsCurrentColor())
    return a.IsCurrentColor() && b.IsCurrentColor();
  return a.GetColor() == b.GetColor();
}

bool operator!=(const StyleColor& a, const StyleColor& b) {
  return !(a == b);
}

}  // namespace blink
