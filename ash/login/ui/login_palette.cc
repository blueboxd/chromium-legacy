// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_palette.h"
#include "ash/public/cpp/login_constants.h"
#include "ui/gfx/color_palette.h"

namespace ash {

LoginPalette CreateDefaultLoginPalette() {
  return LoginPalette(
      {.password_text_color = SK_ColorWHITE,
       .password_placeholder_text_color =
           login_constants::kAuthMethodsTextColor,
       .password_background_color = SK_ColorTRANSPARENT,
       .button_enabled_color = login_constants::kButtonEnabledColor,
       .pin_ink_drop_highlight_color = SkColorSetA(SK_ColorWHITE, 0x0A),
       .pin_ink_drop_ripple_color = SkColorSetA(SK_ColorWHITE, 0x0F),
       .pin_backspace_icon_color = gfx::kGoogleGrey200});
}

LoginPalette CreateInSessionAuthPalette() {
  return LoginPalette(
      {.password_text_color = SK_ColorDKGRAY,
       .password_placeholder_text_color = SK_ColorDKGRAY,
       .password_background_color = SK_ColorTRANSPARENT,
       .button_enabled_color = SK_ColorDKGRAY,
       .pin_ink_drop_highlight_color = SkColorSetA(SK_ColorDKGRAY, 0x0A),
       .pin_ink_drop_ripple_color = SkColorSetA(SK_ColorDKGRAY, 0x0F),
       .pin_backspace_icon_color = SK_ColorDKGRAY});
}

}  // namespace ash
