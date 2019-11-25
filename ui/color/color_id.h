// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_ID_H_
#define UI_COLOR_COLOR_ID_H_

#include "build/build_config.h"

// clang-format off
#define CROSS_PLATFORM_COLOR_IDS \
  /* Core color concepts */ \
  E(kColorAccent, kUiColorsStart), \
  E(kColorAlertHighSeverity), \
  E(kColorAlertLowSeverity), \
  E(kColorAlertMediumSeverity), \
  E(kColorBorderAndSeparatorForeground), \
  E(kColorDisabledForeground), \
  E(kColorItemSelectionBackground), \
  E(kColorPrimaryBackground), \
  E(kColorPrimaryForeground), \
  E(kColorSecondaryForeground), \
  E(kColorSubtleEmphasisBackground), \
  E(kColorTextSelectionBackground), \
  \
  /* Further UI element colors */ \
  E(kColorBubbleBackground), \
  E(kColorBubbleFooterBackground), \
  E(kColorButtonBackground), \
  E(kColorButtonBorder), \
  E(kColorButtonDisabledForeground), \
  E(kColorButtonForeground), \
  E(kColorButtonPressedBackground), \
  E(kColorButtonProminentBackground), \
  E(kColorButtonProminentDisabledBackground), \
  E(kColorButtonProminentFocusedBackground), \
  E(kColorButtonProminentForeground), \
  E(kColorButtonUncheckedForeground), \
  E(kColorDialogBackground), \
  E(kColorDialogForeground), \
  E(kColorFocusableBorderFocused), \
  E(kColorFocusableBorderUnfocused), \
  E(kColorIcon), \
  E(kColorLabelDisabledForeground), \
  E(kColorLabelForeground), \
  E(kColorLabelSecondaryForeground), \
  E(kColorLabelSelectionBackground), \
  E(kColorLabelSelectionForeground), \
  E(kColorLinkDisabledForeground), \
  E(kColorLinkForeground), \
  E(kColorLinkPressedForeground), \
  E(kColorMenuBackground), \
  E(kColorMenuBorder), \
  E(kColorMenuItemAlertedBackground), \
  E(kColorMenuItemDisabledForeground), \
  E(kColorMenuItemForeground), \
  E(kColorMenuItemHighlightedBackground), \
  E(kColorMenuItemHighlightedForeground), \
  E(kColorMenuItemSecondaryForeground), \
  E(kColorMenuItemSelectedBackground), \
  E(kColorMenuItemSelectedForeground), \
  E(kColorMenuSeparator), \
  E(kColorTabContentSeparator), \
  E(kColorTabForeground), \
  E(kColorTabSelectedForeground), \
  E(kColorTableBackground), \
  E(kColorTableForeground), \
  E(kColorTableGroupingIndicator), \
  E(kColorTableHeaderBackground), \
  E(kColorTableHeaderForeground), \
  E(kColorTableHeaderSeparator), \
  E(kColorTableSelectedFocusedBackground), \
  E(kColorTableSelectedFocusedForeground), \
  E(kColorTableSelectedUnfocusedBackground), \
  E(kColorTableSelectedUnfocusedForeground), \
  E(kColorTextfieldBackground), \
  E(kColorTextfieldDisabledBackground), \
  E(kColorTextfieldDisabledForeground), \
  E(kColorTextfieldForeground), \
  E(kColorTextfieldSelectionBackground), \
  E(kColorTextfieldSelectionForeground), \
  E(kColorThrobber), \
  E(kColorTooltipBackground), \
  E(kColorTooltipForeground), \
  E(kColorTreeBackground), \
  E(kColorTreeNodeForeground), \
  E(kColorTreeNodeSelectedFocusedBackground), \
  E(kColorTreeNodeSelectedFocusedForeground), \
  E(kColorTreeNodeSelectedUnfocusedBackground), \
  E(kColorTreeNodeSelectedUnfocusedForeground), \
  E(kColorWindowBackground)

#if defined(OS_WIN)
#define WIN_COLOR_IDS \
  /* Windows native colors */ \
  E(kColorNative3dDkShadow), \
  E(kColorNative3dLight), \
  E(kColorNativeActiveBorder), \
  E(kColorNativeActiveCaption), \
  E(kColorNativeAppWorkspace), \
  E(kColorNativeBackground), \
  E(kColorNativeBtnFace), \
  E(kColorNativeBtnHighlight), \
  E(kColorNativeBtnShadow), \
  E(kColorNativeBtnText), \
  E(kColorNativeCaptionText), \
  E(kColorNativeGradientActiveCaption), \
  E(kColorNativeGradientInactiveCaption), \
  E(kColorNativeGrayText), \
  E(kColorNativeHighlight), \
  E(kColorNativeHighlightText), \
  E(kColorNativeHotlight), \
  E(kColorNativeInactiveBorder), \
  E(kColorNativeInactiveCaption), \
  E(kColorNativeInactiveCaptionText), \
  E(kColorNativeInfoBk), \
  E(kColorNativeInfoText), \
  E(kColorNativeMenu), \
  E(kColorNativeMenuBar), \
  E(kColorNativeMenuHilight), \
  E(kColorNativeMenuText), \
  E(kColorNativeScrollbar), \
  E(kColorNativeWindow), \
  E(kColorNativeWindowFrame), \
  E(kColorNativeWindowText)
#endif

#if defined(OS_WIN)
#define COLOR_IDS \
  CROSS_PLATFORM_COLOR_IDS, \
  WIN_COLOR_IDS
#else
#define COLOR_IDS CROSS_PLATFORM_COLOR_IDS
#endif
// clang-format on

namespace ui {

#include "ui/color/color_id_macros.inc"

// ColorId contains identifiers for all input, intermediary, and output colors
// known to the core UI layer.  Embedders can extend this enum with additional
// values that are understood by the ColorProvider implementation.  Embedders
// define enum values from kUiColorsEnd.  Values named beginning with "kColor"
// represent the actual colors; the rest are markers.
using ColorId = int;
enum ColorIds : ColorId {
  kUiColorsStart = 0,

  COLOR_IDS,

  // TODO(pkasting): Other native colors

  // Embedders must start color IDs from this value.
  kUiColorsEnd,

  // Embedders must not assign IDs larger than this value.  This is used to
  // verify that color IDs and color set IDs are not interchanged.
  kUiColorsLast = 0xffff
};

#include "ui/color/color_id_macros.inc"

// ColorSetId contains identifiers for all distinct color sets known to the core
// UI layer.  As with ColorId, embedders can extend this enum with additional
// values that are understood by the ColorProvider implementation.  Embedders
// define enum values from kUiColorSetsEnd.  Values named beginning with
// "kColorSet" represent the actual colors; the rest are markers.
using ColorSetId = int;
enum ColorSetIds : ColorSetId {
  kUiColorSetsStart = kUiColorsLast + 1,

  // A set of color IDs whose values match the native platform as closely as
  // possible.
  kColorSetNative = kUiColorSetsStart,

  // A set of color IDs representing the default values for core color concepts,
  // in the absence of native colors.
  kColorSetCoreDefaults,

  // Embedders must start color set IDs from this value.
  kUiColorSetsEnd,
};

// Verifies that |id| is a color ID, not a color set ID.
#define DCHECK_COLOR_ID_VALID(id) \
  DCHECK_GE(id, kUiColorsStart);  \
  DCHECK_LE(id, kUiColorsLast)

// Verifies that |id| is a color set ID, not a color ID.
#define DCHECK_COLOR_SET_ID_VALID(id) DCHECK_GE(id, kUiColorSetsStart)

}  // namespace ui

#endif  // UI_COLOR_COLOR_ID_H_
