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

  mixer[kColorBubbleBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorBubbleFooterBackground] =
      FromInputColor(kColorSecondaryBackgroundSubtle);
  mixer[kColorButtonBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorButtonBorder] = FromInputColor(kColorSecondaryBackground);
  mixer[kColorButtonDisabledForeground] =
      FromInputColor(kColorDisabledForeground);
  mixer[kColorButtonProminentBackground] = FromInputColor(kColorAccent);
  mixer[kColorButtonProminentDisabledBackground] = AlphaBlend(
      FromInputColor(kColorSecondaryBackground),
      FromResultColor(kColorButtonBackground), gfx::kDisabledControlAlpha);
  mixer[kColorButtonProminentFocusedBackground] = BlendForMinContrastWithSelf(
      FromResultColor(kColorButtonProminentBackground), 1.3f);
  mixer[kColorButtonProminentForeground] =
      GetColorWithMaxContrast(FromResultColor(kColorButtonProminentBackground));
  mixer[kColorDialogBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorFocusableBorderFocused] =
      SetAlpha(FromInputColor(kColorAccent), 0x4D);
  mixer[kColorFocusableBorderUnfocused] =
      FromInputColor(kColorSecondaryBackground);
  mixer[kColorLabelDisabledForeground] = SetAlpha(
      FromResultColor(kColorLabelForeground), gfx::kDisabledControlAlpha);
  mixer[kColorLabelForeground] = FromInputColor(kColorPrimaryForeground);
  mixer[kColorLabelSelectionBackground] =
      FromInputColor(kColorTextSelectionBackground);
  mixer[kColorLabelSelectionForeground] =
      FromResultColor(kColorLabelForeground);
  mixer[kColorLinkDisabledForeground] = FromInputColor(kColorPrimaryForeground);
  mixer[kColorLinkPressedForeground] = FromInputColor(kColorLinkForeground);
  mixer[kColorMenuBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorMenuBorder] = FromInputColor(kColorSecondaryBackground);
  mixer[kColorMenuItemAlertedBackground] = FromInputColor(kColorAccent);
  mixer[kColorMenuItemDisabledForeground] =
      FromInputColor(kColorDisabledForeground);
  mixer[kColorMenuItemForeground] = FromInputColor(kColorPrimaryForeground);
  mixer[kColorMenuItemHighlightedBackground] =
      FromInputColor(kColorSecondaryBackgroundSubtle);
  mixer[kColorMenuItemHighlightedForeground] =
      FromResultColor(kColorMenuItemForeground);
  mixer[kColorMenuItemSecondaryForeground] =
      FromInputColor(kColorSecondaryForeground);
  mixer[kColorMenuItemSelectedBackground] =
      FromInputColor(kColorSecondaryBackground);
  mixer[kColorMenuItemSelectedForeground] =
      FromResultColor(kColorMenuItemForeground);
  mixer[kColorMenuSeparator] = FromInputColor(kColorSeparatorForeground);
  mixer[kColorTabContentSeparator] = FromInputColor(kColorSecondaryBackground);
  mixer[kColorTabForeground] = FromInputColor(kColorSecondaryForeground);
  mixer[kColorTabSelectedForeground] = FromInputColor(kColorAccent);
  mixer[kColorTableBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorTableForeground] = FromInputColor(kColorPrimaryForeground);
  mixer[kColorTableGroupingIndicator] =
      FromResultColor(kColorTableSelectedFocusedBackground);
  mixer[kColorTableHeaderBackground] = FromResultColor(kColorTableBackground);
  mixer[kColorTableHeaderForeground] = FromResultColor(kColorTableForeground);
  mixer[kColorTableHeaderSeparator] = FromInputColor(kColorSeparatorForeground);
  mixer[kColorTableSelectedFocusedBackground] =
      FromInputColor(kColorSecondaryBackground);
  mixer[kColorTableSelectedFocusedForeground] =
      FromResultColor(kColorTableForeground);
  mixer[kColorTableSelectedUnfocusedBackground] =
      FromResultColor(kColorTableSelectedFocusedBackground);
  mixer[kColorTableSelectedUnfocusedForeground] =
      FromResultColor(kColorTableSelectedFocusedForeground);
  mixer[kColorTextfieldBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorTextfieldDisabledBackground] =
      FromResultColor(kColorTextfieldBackground);
  mixer[kColorTextfieldDisabledForeground] = SetAlpha(
      FromResultColor(kColorTextfieldForeground), gfx::kDisabledControlAlpha);
  mixer[kColorTextfieldForeground] = FromInputColor(kColorPrimaryForeground);
  mixer[kColorTextfieldSelectionBackground] =
      FromInputColor(kColorTextSelectionBackground);
  mixer[kColorTextfieldSelectionForeground] =
      FromResultColor(kColorTextfieldForeground);
  mixer[kColorThrobber] = FromInputColor(kColorAccent);
  mixer[kColorTooltipBackground] = SetAlpha(
      GetColorWithMaxContrast(FromInputColor(kColorPrimaryBackground)), 0xE9);
  mixer[kColorTooltipForeground] = SetAlpha(
      GetColorWithMaxContrast(FromResultColor(kColorTooltipBackground)), 0xDE);
  mixer[kColorTreeBackground] = FromInputColor(kColorPrimaryBackground);
  mixer[kColorTreeNodeForeground] = FromInputColor(kColorPrimaryForeground);
  mixer[kColorTreeNodeSelectedFocusedBackground] =
      FromInputColor(kColorSecondaryBackground);
  mixer[kColorTreeNodeSelectedFocusedForeground] =
      FromResultColor(kColorTreeNodeForeground);
  mixer[kColorTreeNodeSelectedUnfocusedBackground] =
      FromResultColor(kColorTreeNodeSelectedFocusedBackground);
  mixer[kColorTreeNodeSelectedUnfocusedForeground] =
      FromResultColor(kColorTreeNodeSelectedFocusedForeground);
  mixer[kColorWindowBackground] = FromInputColor(kColorPrimaryBackground);
}

}  // namespace ui
