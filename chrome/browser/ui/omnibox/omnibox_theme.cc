// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_theme.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "ui/native_theme/native_theme_dark_aura.h"
#endif

namespace {

SkColor GetColorFromNativeTheme(ui::NativeTheme::ColorId color_id,
                                OmniboxTint tint) {
  ui::NativeTheme* native_theme = nullptr;
#if defined(USE_AURA) || defined(OS_MACOSX)
  if (tint == OmniboxTint::DARK)
    native_theme = ui::NativeThemeDarkAura::instance();
#endif
  if (!native_theme)
    native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  return native_theme->GetSystemColor(color_id);
}

SkColor GetSecurityChipColor(OmniboxTint tint, OmniboxPartState state) {
  if (tint == OmniboxTint::DARK)
    return gfx::kGoogleGrey200;
  return (state == OmniboxPartState::CHIP_DANGEROUS) ? gfx::kGoogleRed600
                                                     : gfx::kChromeIconGrey;
}

}  // namespace

SkColor GetOmniboxColor(OmniboxPart part,
                        OmniboxTint tint,
                        OmniboxPartState state) {
  using NativeId = ui::NativeTheme::ColorId;

  bool dark = tint == OmniboxTint::DARK;

  // For high contrast, selected rows use inverted colors to stand out more.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  bool high_contrast = native_theme && native_theme->UsesHighContrastColors();
  if (high_contrast && (state == OmniboxPartState::SELECTED))
    dark = !dark;

  switch (part) {
    case OmniboxPart::LOCATION_BAR_BACKGROUND:
      if (state == OmniboxPartState::HOVERED) {
        return color_utils::BlendTowardMaxContrast(
            GetOmniboxColor(part, tint, OmniboxPartState::NORMAL), 0x0a);
      }
      return dark ? SkColorSetRGB(0x28, 0x2C, 0x2F) : gfx::kGoogleGrey100;
    case OmniboxPart::LOCATION_BAR_SECURITY_CHIP:
      return GetSecurityChipColor(tint, state);
    case OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD:
      return dark ? gfx::kGoogleGrey100 : gfx::kGoogleBlue600;
    case OmniboxPart::RESULTS_BACKGROUND: {
      const SkColor base_color = dark ? gfx::kGoogleGrey900 : SK_ColorWHITE;
      // The spec calls for transparent black (or white) overlays for hover
      // (10%) and select (16%). Pre-blend these with the background for the
      // best text AA result.
      return color_utils::BlendTowardMaxContrast(
          base_color,
          gfx::ToRoundedInt(GetOmniboxStateOpacity(state) * SK_AlphaOPAQUE));
    }
    case OmniboxPart::LOCATION_BAR_CLEAR_ALL:
    case OmniboxPart::LOCATION_BAR_TEXT_DEFAULT:
    case OmniboxPart::RESULTS_TEXT_DEFAULT:
      return dark ? SK_ColorWHITE : gfx::kGoogleGrey900;

    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
    case OmniboxPart::RESULTS_ICON:
    case OmniboxPart::RESULTS_TEXT_DIMMED:
      // This is a pre-lightened (or darkened) variant of the base text color.
      return dark ? SkColorSetRGB(0x8D, 0x93, 0x99)
                  : SkColorSetRGB(0x6B, 0x6F, 0x74);

    case OmniboxPart::RESULTS_TEXT_URL:
      if (high_contrast)
        return dark ? gfx::kGoogleBlue200 : gfx::kGoogleBlue900;
      return dark ? gfx::kGoogleBlue300 : gfx::kGoogleBlue800;

    case OmniboxPart::LOCATION_BAR_BUBBLE_OUTLINE:
      if (OmniboxFieldTrial::IsExperimentalKeywordModeEnabled())
        return gfx::kGoogleBlue700;
      return dark ? gfx::kGoogleGrey100
                  : SkColorSetA(gfx::kGoogleGrey900, 0x24);

    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_BACKGROUND:
      return GetColorFromNativeTheme(
          NativeId::kColorId_TextfieldSelectionBackgroundFocused, tint);

    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_TEXT:
      return GetColorFromNativeTheme(NativeId::kColorId_TextfieldSelectionColor,
                                     tint);
  }
  return gfx::kPlaceholderColor;
}

SkColor GetOmniboxSecurityChipColor(
    OmniboxTint tint,
    security_state::SecurityLevel security_level) {
  if (security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT) {
    return GetOmniboxColor(OmniboxPart::LOCATION_BAR_TEXT_DIMMED, tint);
  }

  OmniboxPartState state = OmniboxPartState::CHIP_DEFAULT;
  if (security_level == security_state::EV_SECURE ||
      security_level == security_state::SECURE) {
    state = OmniboxPartState::CHIP_SECURE;
  } else if (security_level == security_state::DANGEROUS) {
    state = OmniboxPartState::CHIP_DANGEROUS;
  }

  return GetOmniboxColor(OmniboxPart::LOCATION_BAR_SECURITY_CHIP, tint, state);
}

float GetOmniboxStateOpacity(OmniboxPartState state) {
  DCHECK_LE(state, OmniboxPartState::SELECTED);
  constexpr float kOpacities[3] = {0.00f, 0.10f, 0.16f};
  return kOpacities[static_cast<size_t>(state)];
}
