// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

void AddOmniboxColorMixer(ui::ColorProvider* provider, bool high_contrast) {
  ui::ColorMixer& mixer = provider->AddMixer();
  const float minimum_contrast =
      high_contrast ? 6.0f : color_utils::kMinimumReadableContrastRatio;

  // Omnibox background colors.
  mixer[kColorOmniboxBackground] = ui::GetResultingPaintColor(
      ui::FromTransformInput(), ui::FromInputColor(kColorToolbar));
  mixer[kColorOmniboxBackgroundHovered] = ui::BlendTowardMaxContrast(
      ui::FromResultColor(kColorOmniboxBackground), 0x0A);

  // Omnibox text colors.
  mixer[kColorOmniboxText] = ui::GetResultingPaintColor(
      ui::FromTransformInput(), ui::FromResultColor(kColorOmniboxBackground));
  {
    auto& selected_text = mixer[kColorOmniboxResultsTextSelected];
    selected_text = ui::FromResultColor(kColorOmniboxText);
    if (high_contrast)
      selected_text += ui::ContrastInvert(ui::FromTransformInput());
  }
  mixer[kColorOmniboxSelectedKeyword] = ui::SelectBasedOnDarkInput(
      ui::FromResultColor(kColorOmniboxBackground), gfx::kGoogleGrey100,
      ui::FromResultColor(kColorOmniboxResultsUrl));

  // Bubble outline colors.
  mixer[kColorOmniboxBubbleOutline] = ui::SelectBasedOnDarkInput(
      ui::FromResultColor(kColorOmniboxBackground), gfx::kGoogleGrey100,
      SkColorSetA(gfx::kGoogleGrey900, 0x24));
  mixer[kColorOmniboxBubbleOutlineExperimentalKeywordMode] =
      ui::FromResultColor(kColorOmniboxSelectedKeyword);

  // Results background colors.
  mixer[kColorOmniboxResultsBackground] =
      ui::GetColorWithMaxContrast(ui::FromResultColor(kColorOmniboxText));
  mixer[kColorOmniboxResultsBackgroundHovered] = ui::BlendTowardMaxContrast(
      ui::FromResultColor(kColorOmniboxResultsBackground),
      gfx::kGoogleGreyAlpha200);
  mixer[kColorOmniboxResultsBackgroundSelected] = ui::BlendTowardMaxContrast(
      ui::GetColorWithMaxContrast(
          ui::FromResultColor(kColorOmniboxResultsTextSelected)),
      gfx::kGoogleGreyAlpha300);

  // Results icon colors.
  {
    const auto results_icon = [minimum_contrast](ui::ColorId text_id,
                                                 ui::ColorId background_id) {
      return ui::BlendForMinContrast(
          ui::DeriveDefaultIconColor(ui::FromResultColor(text_id)),
          ui::FromResultColor(background_id), base::nullopt, minimum_contrast);
    };
    mixer[kColorOmniboxResultsIcon] =
        results_icon(kColorOmniboxText, kColorOmniboxResultsBackground);
    mixer[kColorOmniboxResultsIconSelected] =
        results_icon(kColorOmniboxResultsTextSelected,
                     kColorOmniboxResultsBackgroundSelected);
  }

  // Dimmed text colors.
  {
    const auto blend_with_clamped_contrast = [minimum_contrast](
                                                 ui::ColorId foreground_id,
                                                 ui::ColorId background_id) {
      return ui::BlendForMinContrast(
          ui::FromResultColor(foreground_id),
          ui::FromResultColor(foreground_id),
          ui::BlendForMinContrast(ui::FromResultColor(background_id),
                                  ui::FromResultColor(background_id),
                                  base::nullopt, minimum_contrast),
          minimum_contrast);
    };
    mixer[kColorOmniboxResultsTextDimmed] = blend_with_clamped_contrast(
        kColorOmniboxText, kColorOmniboxResultsBackgroundHovered);
    mixer[kColorOmniboxResultsTextDimmedSelected] =
        blend_with_clamped_contrast(kColorOmniboxResultsTextSelected,
                                    kColorOmniboxResultsBackgroundSelected);
    mixer[kColorOmniboxTextDimmed] = blend_with_clamped_contrast(
        kColorOmniboxText, kColorOmniboxBackgroundHovered);
  }

  // Results URL colors.
  {
    const auto url_color = [minimum_contrast](ui::ColorId id) {
      return ui::BlendForMinContrast(
          gfx::kGoogleBlue500, ui::FromResultColor(id),
          ui::SelectBasedOnDarkInput(ui::FromResultColor(id),
                                     gfx::kGoogleBlue050, gfx::kGoogleBlue900),
          minimum_contrast);
    };
    mixer[kColorOmniboxResultsUrl] =
        url_color(kColorOmniboxResultsBackgroundHovered);
    mixer[kColorOmniboxResultsUrlSelected] =
        url_color(kColorOmniboxResultsBackgroundSelected);
  }

  // Security chip colors.
  {
    const auto security_chip_color =
        [minimum_contrast](ui::ColorTransform transform) {
          return ui::SelectBasedOnDarkInput(
              ui::FromResultColor(kColorOmniboxBackground),
              ui::BlendTowardMaxContrast(ui::FromResultColor(kColorOmniboxText),
                                         0x18),
              ui::BlendForMinContrast(
                  std::move(transform),
                  ui::FromResultColor(kColorOmniboxBackgroundHovered),
                  base::nullopt, minimum_contrast));
        };
    mixer[kColorOmniboxSecurityChipDangerous] =
        security_chip_color(gfx::kGoogleRed600);
    mixer[kColorOmniboxSecurityChipSecure] = security_chip_color(
        ui::DeriveDefaultIconColor(ui::FromResultColor(kColorOmniboxText)));
  }
  mixer[kColorOmniboxSecurityChipDefault] =
      ui::FromResultColor(kColorOmniboxSecurityChipSecure);
}
