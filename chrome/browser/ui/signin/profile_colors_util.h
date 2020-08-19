// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_PROFILE_COLORS_UTIL_H_
#define CHROME_BROWSER_UI_SIGNIN_PROFILE_COLORS_UTIL_H_

#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/common/search/generated_colors_info.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

// Gets the profile theme colors associated with a profile. Does not support
// incognito or guest profiles.
ProfileThemeColors GetThemeColorsForProfile(Profile* profile);

// Returns the color that should be used to display text over the profile
// highlight color.
SkColor GetProfileForegroundTextColor(SkColor profile_highlight_color);

// Returns the color that should be used to display icons over the profile
// highlight color.
SkColor GetProfileForegroundIconColor(SkColor profile_highlight_color);

// Returns the color that should be used to generate the default avatar icon.
SkColor GetAvatarStrokeColor(SkColor avatar_fill_color);

// Returns a new color for a profile, based on the colors of the existing
// profiles.
chrome_colors::ColorInfo GenerateNewProfileColor();

#endif  // CHROME_BROWSER_UI_SIGNIN_PROFILE_COLORS_UTIL_H_
