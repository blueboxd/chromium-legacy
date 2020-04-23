// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "build/build_config.h"
#include "ui/color/color_mixers.h"

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {

void AddUiColorMixer(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBubbleBackground] = {kColorPrimaryBackground};
  mixer[kColorBubbleFooterBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackground] = {kColorPrimaryBackground};
  mixer[kColorButtonBorder] = {kColorBorderAndSeparatorForeground};
  mixer[kColorButtonDisabledForeground] = {kColorDisabledForeground};
  mixer[kColorButtonForeground] = {kColorAccent};
  mixer[kColorButtonPressedBackground] = {kColorButtonBackground};
  mixer[kColorButtonProminentBackground] = {kColorAccent};
  mixer[kColorButtonProminentDisabledBackground] =
      BlendForMinContrastWithSelf(kColorButtonBackground, 1.2f);
  mixer[kColorButtonProminentFocusedBackground] =
      BlendForMinContrastWithSelf(kColorButtonProminentBackground, 1.3f);
  mixer[kColorButtonProminentForeground] =
      GetColorWithMaxContrast(kColorButtonProminentBackground);
  mixer[kColorButtonUncheckedForeground] = {kColorSecondaryForeground};
  mixer[kColorDialogBackground] = {kColorPrimaryBackground};
  mixer[kColorDialogForeground] = {kColorSecondaryForeground};
  mixer[kColorFocusableBorderFocused] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorFocusableBorderUnfocused] = {kColorBorderAndSeparatorForeground};
  mixer[kColorIcon] = {kColorSecondaryForeground};
  mixer[kColorMenuIcon] = {kColorIcon};
  mixer[kColorLabelDisabledForeground] = {kColorDisabledForeground};
  mixer[kColorLabelForeground] = {kColorPrimaryForeground};
  mixer[kColorLabelSecondaryForeground] = {kColorSecondaryForeground};
  mixer[kColorLabelSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorLabelSelectionForeground] = {kColorLabelForeground};
  mixer[kColorLinkDisabledForeground] = {kColorDisabledForeground};
  mixer[kColorLinkPressedForeground] = {kColorLinkForeground};
  mixer[kColorLinkForeground] = {kColorAccent};
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuBorder] = {kColorBorderAndSeparatorForeground};
  mixer[kColorMenuItemBackgroundAlertedInitial] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorMenuItemBackgroundAlertedTarget] = SetAlpha(kColorAccent, 0x1A);
  mixer[kColorMenuItemDisabledForeground] = {kColorDisabledForeground};
  mixer[kColorMenuItemForeground] = {kColorPrimaryForeground};
  mixer[kColorMenuItemHighlightedBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorMenuItemHighlightedForeground] = {kColorMenuItemForeground};
  mixer[kColorMenuItemSecondaryForeground] = {kColorSecondaryForeground};
  mixer[kColorMenuItemSelectedBackground] = {kColorItemSelectionBackground};
  mixer[kColorMenuItemSelectedForeground] = {kColorMenuItemForeground};
  mixer[kColorMenuSeparator] = {kColorBorderAndSeparatorForeground};
  mixer[kColorTabContentSeparator] = {kColorBorderAndSeparatorForeground};
  mixer[kColorTabForeground] = {kColorSecondaryForeground};
  mixer[kColorTabSelectedForeground] = {kColorAccent};
  mixer[kColorTableBackground] = {kColorPrimaryBackground};
  mixer[kColorTableForeground] = {kColorPrimaryForeground};
  mixer[kColorTableGroupingIndicator] = {kColorTableSelectedFocusedBackground};
  mixer[kColorTableHeaderBackground] = {kColorTableBackground};
  mixer[kColorTableHeaderForeground] = {kColorTableForeground};
  mixer[kColorTableHeaderSeparator] = {kColorBorderAndSeparatorForeground};
  mixer[kColorTableSelectedFocusedBackground] = {kColorItemSelectionBackground};
  mixer[kColorTableSelectedFocusedForeground] = {kColorTableForeground};
  mixer[kColorTableSelectedUnfocusedBackground] = {
      kColorTableSelectedFocusedBackground};
  mixer[kColorTableSelectedUnfocusedForeground] = {
      kColorTableSelectedFocusedForeground};
  mixer[kColorTextfieldBackground] =
      GetColorWithMaxContrast(kColorTextfieldForeground);
  mixer[kColorTextfieldDisabledBackground] = {kColorPrimaryBackground};
  mixer[kColorTextfieldDisabledForeground] = {kColorDisabledForeground};
  mixer[kColorTextfieldPlaceholderForeground] = {
      kColorTextfieldDisabledForeground};
  mixer[kColorTextfieldForeground] = {kColorPrimaryForeground};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextfieldForeground};
  mixer[kColorThrobber] = {kColorAccent};
  mixer[kColorTooltipBackground] = SetAlpha(kColorPrimaryBackground, 0xCC);
  mixer[kColorTooltipForeground] = SetAlpha(kColorPrimaryForeground, 0xDE);
  mixer[kColorTreeBackground] = {kColorPrimaryBackground};
  mixer[kColorTreeNodeForeground] = {kColorPrimaryForeground};
  mixer[kColorTreeNodeSelectedFocusedBackground] = {
      kColorItemSelectionBackground};
  mixer[kColorTreeNodeSelectedFocusedForeground] = {kColorTreeNodeForeground};
  mixer[kColorTreeNodeSelectedUnfocusedBackground] = {
      kColorTreeNodeSelectedFocusedBackground};
  mixer[kColorTreeNodeSelectedUnfocusedForeground] = {
      kColorTreeNodeSelectedFocusedForeground};
  mixer[kColorWindowBackground] = {kColorPrimaryBackground};
}

}  // namespace ui
