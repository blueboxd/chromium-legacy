// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/profile_colors_util.h"

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/gfx/color_utils.h"

ProfileThemeColors GetThemeColorsForProfile(Profile* profile) {
  DCHECK(profile->IsRegularProfile());
  ProfileAttributesEntry* entry = nullptr;
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile->GetPath(), &entry);
  DCHECK(entry);
  return entry->GetProfileThemeColors();
}

SkColor GetProfileForegroundTextColor(SkColor profile_highlight_color) {
  return color_utils::GetColorWithMaxContrast(profile_highlight_color);
}

SkColor GetProfileForegroundIconColor(SkColor profile_highlight_color) {
  SkColor text_color = GetProfileForegroundTextColor(profile_highlight_color);
  SkColor icon_color = color_utils::DeriveDefaultIconColor(text_color);
  return color_utils::BlendForMinContrast(icon_color, profile_highlight_color,
                                          text_color)
      .color;
}

SkColor GetAvatarStrokeColor(SkColor avatar_fill_color) {
  if (color_utils::IsDark(avatar_fill_color)) {
    return SK_ColorWHITE;
  }

  color_utils::HSL color_hsl;
  color_utils::SkColorToHSL(avatar_fill_color, &color_hsl);
  color_hsl.l = std::max(0., color_hsl.l - 0.5);
  return color_utils::HSLToSkColor(color_hsl, SkColorGetA(avatar_fill_color));
}

chrome_colors::ColorInfo GenerateNewProfileColor() {
  // TODO(crbug.com/1108295):
  // - Implement more sophisticated algorithm to pick the new profile color.
  // - Return only a SkColor if the full ColorInfo is not needed.
  size_t size = base::size(chrome_colors::kGeneratedColorsInfo);
  size_t index = static_cast<size_t>(base::RandInt(0, size - 1));
  return chrome_colors::kGeneratedColorsInfo[index];
}
