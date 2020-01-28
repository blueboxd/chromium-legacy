// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/increased_contrast_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"

IncreasedContrastThemeSupplier::IncreasedContrastThemeSupplier(
    bool is_dark_mode)
    : CustomThemeSupplier(INCREASED_CONTRAST), is_dark_mode_(is_dark_mode) {}
IncreasedContrastThemeSupplier::~IncreasedContrastThemeSupplier() {}

// TODO(ellyjones): Follow up with a11y designers about these color choices.
bool IncreasedContrastThemeSupplier::GetColor(int id, SkColor* color) const {
  const SkColor foreground = is_dark_mode_ ? SK_ColorWHITE : SK_ColorBLACK;
  const SkColor background = is_dark_mode_ ? SK_ColorBLACK : SK_ColorWHITE;
  switch (id) {
    case ThemeProperties::COLOR_TAB_TEXT:
      *color = foreground;
      return true;
    case ThemeProperties::COLOR_BACKGROUND_TAB_TEXT:
    case ThemeProperties::COLOR_BACKGROUND_TAB_TEXT_INACTIVE:
    case ThemeProperties::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO:
    case ThemeProperties::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO_INACTIVE:
      *color = foreground;
      return true;
    case ThemeProperties::COLOR_TOOLBAR:
      *color = background;
      return true;
    case ThemeProperties::COLOR_FRAME_INACTIVE:
    case ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE:
      *color = SK_ColorGRAY;
      return true;
    case ThemeProperties::COLOR_FRAME:
    case ThemeProperties::COLOR_FRAME_INCOGNITO:
      *color = SK_ColorDKGRAY;
      return true;
    case ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR:
      *color = is_dark_mode_ ? SK_ColorDKGRAY : SK_ColorLTGRAY;
      return true;
    case ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR:
      *color = foreground;
      return true;
    case ThemeProperties::COLOR_LOCATION_BAR_BORDER:
      *color = foreground;
      return true;
  }
  return false;
}

bool IncreasedContrastThemeSupplier::CanUseIncognitoColors() const {
  return false;
}
