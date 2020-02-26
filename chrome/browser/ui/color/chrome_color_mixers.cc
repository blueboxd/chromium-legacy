// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixers.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

void AddChromeColorMixers(ui::ColorProvider* provider) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorOmniboxBackground] = ui::BlendForMinContrast(
      ui::kColorTextfieldBackground, kColorToolbar, base::nullopt, 1.11f);
  mixer[kColorOmniboxText] = {ui::kColorTextfieldForeground};
  // TODO(tluk) Behavior change for dark mode to a darker toolbar color for
  // better color semantics. Follow up with UX team before landing change.
  mixer[kColorToolbar] = {ui::kColorPrimaryBackground};
}
