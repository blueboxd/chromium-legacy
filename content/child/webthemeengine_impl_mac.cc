// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_mac.h"

#include "content/child/webthemeengine_impl_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace content {

blink::ForcedColors WebThemeEngineMac::GetForcedColors() const {
  return forced_colors_;
}

void WebThemeEngineMac::SetForcedColors(
    const blink::ForcedColors forced_colors) {
  forced_colors_ = forced_colors;
}

blink::PreferredColorScheme WebThemeEngineMac::PreferredColorScheme() const {
  ui::NativeTheme::PreferredColorScheme preferred_color_scheme =
      ui::NativeTheme::GetInstanceForWeb()->GetPreferredColorScheme();
  return WebPreferredColorScheme(preferred_color_scheme);
}

void WebThemeEngineMac::SetPreferredColorScheme(
    const blink::PreferredColorScheme preferred_color_scheme) {
  ui::NativeTheme::GetInstanceForWeb()->set_preferred_color_scheme(
      NativePreferredColorScheme(preferred_color_scheme));
}

}  // namespace content
