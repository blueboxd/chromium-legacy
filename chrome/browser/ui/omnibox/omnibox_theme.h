// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_

#include "components/security_state/core/security_state.h"
#include "third_party/skia/include/core/SkColor.h"

// A part of the omnibox (location bar, location bar decoration, or dropdown).
enum class OmniboxPart {
  LOCATION_BAR_BACKGROUND,
  LOCATION_BAR_CLEAR_ALL,
  LOCATION_BAR_IME_AUTOCOMPLETE_BACKGROUND,
  LOCATION_BAR_IME_AUTOCOMPLETE_TEXT,
  LOCATION_BAR_SECURITY_CHIP,
  LOCATION_BAR_SELECTED_KEYWORD,
  LOCATION_BAR_TEXT_DEFAULT,
  LOCATION_BAR_TEXT_DIMMED,
  LOCATION_BAR_BUBBLE_OUTLINE,

  RESULTS_BACKGROUND,  // Background of the results dropdown.
  RESULTS_ICON,
  RESULTS_TEXT_DEFAULT,
  RESULTS_TEXT_DIMMED,
  RESULTS_TEXT_URL,
};

// The tint of the omnibox theme. E.g. Incognito may use a DARK tint.
enum class OmniboxTint { DARK, LIGHT };

// An optional state for a given |OmniboxPart|.
enum class OmniboxPartState {
  NORMAL,
  HOVERED,
  SELECTED,

  // Applicable to LOCATION_BAR_SECURITY_CHIP only.
  CHIP_DEFAULT,
  CHIP_DANGEROUS,
  CHIP_SECURE
};

// Returns the color for the given |part| and |tint|. An optional |state| can be
// provided for OmniboxParts that support stateful colors.
SkColor GetOmniboxColor(OmniboxPart part,
                        OmniboxTint tint,
                        OmniboxPartState state = OmniboxPartState::NORMAL);

// Returns the color of the security chip given |tint| and |security_level|.
SkColor GetOmniboxSecurityChipColor(
    OmniboxTint tint,
    security_state::SecurityLevel security_level);

float GetOmniboxStateOpacity(OmniboxPartState state);

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
