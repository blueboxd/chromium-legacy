// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_INCREASED_CONTRAST_THEME_SUPPLIER_H_
#define CHROME_BROWSER_THEMES_INCREASED_CONTRAST_THEME_SUPPLIER_H_

#include "chrome/browser/themes/custom_theme_supplier.h"

// A theme supplier that maximizes the contrast between UI elements and
// especially the visual prominence of key UI elements (omnibox, active vs
// inactive tab distinction).
class IncreasedContrastThemeSupplier : public CustomThemeSupplier {
 public:
  explicit IncreasedContrastThemeSupplier(bool is_dark_mode);

  bool GetColor(int id, SkColor* color) const override;
  bool CanUseIncognitoColors() const override;

 protected:
  ~IncreasedContrastThemeSupplier() override;

 private:
  bool is_dark_mode_;

  DISALLOW_COPY_AND_ASSIGN(IncreasedContrastThemeSupplier);
};

#endif  // CHROME_BROWSER_THEMES_INCREASED_CONTRAST_THEME_SUPPLIER_H_
