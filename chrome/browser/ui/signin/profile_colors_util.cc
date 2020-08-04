// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/profile_colors_util.h"

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
