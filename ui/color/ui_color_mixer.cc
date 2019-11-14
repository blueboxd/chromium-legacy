// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {

void AddUiColorMixers(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBubbleBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorBubbleFooterBackground].AddTransform(
      FromInputColor(kColorSecondaryBackgroundSubtle));
  mixer[kColorButtonBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorButtonBorder].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorButtonDisabledForeground].AddTransform(
      FromInputColor(kColorDisabledForeground));
  mixer[kColorButtonProminentBackground].AddTransform(
      FromInputColor(kColorAccent));
  mixer[kColorButtonProminentDisabledBackground].AddTransform(AlphaBlend(
      FromInputColor(kColorSecondaryBackground),
      FromResultColor(kColorButtonBackground), gfx::kDisabledControlAlpha));
  mixer[kColorButtonProminentFocusedBackground].AddTransform(
      BlendForMinContrastWithSelf(
          FromResultColor(kColorButtonProminentBackground), 1.3f));
  mixer[kColorButtonProminentForeground].AddTransform(GetColorWithMaxContrast(
      FromResultColor(kColorButtonProminentBackground)));
  mixer[kColorDialogBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorFocusableBorderFocused].AddTransform(
      SetAlpha(FromInputColor(kColorAccent), 0x4D));
  mixer[kColorFocusableBorderUnfocused].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorLabelDisabledForeground].AddTransform(SetAlpha(
      FromResultColor(kColorLabelForeground), gfx::kDisabledControlAlpha));
  mixer[kColorLabelForeground].AddTransform(
      FromInputColor(kColorPrimaryForeground));
  mixer[kColorLabelSelectionBackground].AddTransform(
      FromInputColor(kColorTextSelectionBackground));
  mixer[kColorLabelSelectionForeground].AddTransform(
      FromResultColor(kColorLabelForeground));
  mixer[kColorLinkDisabledForeground].AddTransform(
      FromInputColor(kColorPrimaryForeground));
  mixer[kColorLinkPressedForeground].AddTransform(
      FromInputColor(kColorLinkForeground));
  mixer[kColorMenuBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorMenuBorder].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorMenuItemAlertedBackground].AddTransform(
      FromInputColor(kColorAccent));
  mixer[kColorMenuItemDisabledForeground].AddTransform(
      FromInputColor(kColorDisabledForeground));
  mixer[kColorMenuItemForeground].AddTransform(
      FromInputColor(kColorPrimaryForeground));
  mixer[kColorMenuItemHighlightedBackground].AddTransform(
      FromInputColor(kColorSecondaryBackgroundSubtle));
  mixer[kColorMenuItemHighlightedForeground].AddTransform(
      FromResultColor(kColorMenuItemForeground));
  mixer[kColorMenuItemSecondaryForeground].AddTransform(
      FromInputColor(kColorSecondaryForeground));
  mixer[kColorMenuItemSelectedBackground].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorMenuItemSelectedForeground].AddTransform(
      FromResultColor(kColorMenuItemForeground));
  mixer[kColorMenuSeparator].AddTransform(
      FromInputColor(kColorSeparatorForeground));
  mixer[kColorTabContentSeparator].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorTabForeground].AddTransform(
      FromInputColor(kColorSecondaryForeground));
  mixer[kColorTabSelectedForeground].AddTransform(FromInputColor(kColorAccent));
  mixer[kColorTableBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorTableForeground].AddTransform(
      FromInputColor(kColorPrimaryForeground));
  mixer[kColorTableGroupingIndicator].AddTransform(
      FromResultColor(kColorTableSelectedFocusedBackground));
  mixer[kColorTableHeaderBackground].AddTransform(
      FromResultColor(kColorTableBackground));
  mixer[kColorTableHeaderForeground].AddTransform(
      FromResultColor(kColorTableForeground));
  mixer[kColorTableHeaderSeparator].AddTransform(
      FromInputColor(kColorSeparatorForeground));
  mixer[kColorTableSelectedFocusedBackground].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorTableSelectedFocusedForeground].AddTransform(
      FromResultColor(kColorTableForeground));
  mixer[kColorTableSelectedUnfocusedBackground].AddTransform(
      FromResultColor(kColorTableSelectedFocusedBackground));
  mixer[kColorTableSelectedUnfocusedForeground].AddTransform(
      FromResultColor(kColorTableSelectedFocusedForeground));
  mixer[kColorTextfieldBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorTextfieldDisabledBackground].AddTransform(
      FromResultColor(kColorTextfieldBackground));
  mixer[kColorTextfieldDisabledForeground].AddTransform(SetAlpha(
      FromResultColor(kColorTextfieldForeground), gfx::kDisabledControlAlpha));
  mixer[kColorTextfieldForeground].AddTransform(
      FromInputColor(kColorPrimaryForeground));
  mixer[kColorTextfieldSelectionBackground].AddTransform(
      FromInputColor(kColorTextSelectionBackground));
  mixer[kColorTextfieldSelectionForeground].AddTransform(
      FromResultColor(kColorTextfieldForeground));
  mixer[kColorThrobber].AddTransform(FromInputColor(kColorAccent));
  mixer[kColorTooltipBackground].AddTransform(SetAlpha(
      GetColorWithMaxContrast(FromInputColor(kColorPrimaryBackground)), 0xE9));
  mixer[kColorTooltipForeground].AddTransform(SetAlpha(
      GetColorWithMaxContrast(FromResultColor(kColorTooltipBackground)), 0xDE));
  mixer[kColorTreeBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
  mixer[kColorTreeNodeForeground].AddTransform(
      FromInputColor(kColorPrimaryForeground));
  mixer[kColorTreeNodeSelectedFocusedBackground].AddTransform(
      FromInputColor(kColorSecondaryBackground));
  mixer[kColorTreeNodeSelectedFocusedForeground].AddTransform(
      FromResultColor(kColorTreeNodeForeground));
  mixer[kColorTreeNodeSelectedUnfocusedBackground].AddTransform(
      FromResultColor(kColorTreeNodeSelectedFocusedBackground));
  mixer[kColorTreeNodeSelectedUnfocusedForeground].AddTransform(
      FromResultColor(kColorTreeNodeSelectedFocusedForeground));
  mixer[kColorWindowBackground].AddTransform(
      FromInputColor(kColorPrimaryBackground));
}

}  // namespace ui
