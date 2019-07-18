// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONSTANTS_H_
#define ASH_SHELF_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"
#include "chromeos/constants/chromeos_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ash {

// TODO: Once the new shelf UI is on everywhere, clean-up duplicate constants.

// Size of the shelf when visible (height when the shelf is horizontal and
// width when the shelf is vertical).
constexpr int kShelfSize = 56;
constexpr int kShelfSizeDense = 48;

// Size of the icons within shelf buttons.
constexpr int kShelfButtonIconSize = 44;
constexpr int kShelfButtonIconSizeDense = 36;

// Size for controls like the home button, back button, etc.
constexpr int kShelfControlSize = 40;
constexpr int kShelfControlSizeDense = 36;

ASH_EXPORT constexpr SkColor kShelfControlPermanentHighlightBackground =
    SkColorSetA(SK_ColorWHITE, 26);  // 10%

constexpr SkColor kShelfFocusBorderColor = gfx::kGoogleBlue300;

// We reserve a small area on the edge of the workspace area to ensure that
// the resize handle at the edge of the window can be hit.
constexpr int kWorkspaceAreaVisibleInset = 2;

// When autohidden we extend the touch hit target onto the screen so that the
// user can drag the shelf out.
constexpr int kWorkspaceAreaAutoHideInset = 5;

// Portion of the shelf that's within the screen bounds when auto-hidden.
ASH_EXPORT constexpr int kHiddenShelfInScreenPortion = 3;

// The default base color of the shelf to which different alpha values are
// applied based on the desired shelf opacity level.
ASH_EXPORT constexpr SkColor kShelfDefaultBaseColor = gfx::kGoogleGrey900;

// Size allocated for each app button on the shelf.
ASH_EXPORT constexpr int kShelfButtonSize = kShelfSize;
ASH_EXPORT constexpr int kShelfButtonSizeDense = kShelfSizeDense;

// Size of the space between buttons on the shelf.
ASH_EXPORT constexpr int kShelfButtonSpacing = 8;

// Size of the space between edge of screen and home button.
ASH_EXPORT constexpr int kShelfHomeButtonEdgeSpacing = 8;
ASH_EXPORT constexpr int kShelfHomeButtonEdgeSpacingDense = 6;

// The margin around the overflow button on the shelf.
constexpr int kShelfOverflowButtonMargin =
    (kShelfButtonSize - kShelfControlSize) / 2;
constexpr int kShelfOverflowButtonMarginDense =
    (kShelfButtonSizeDense - kShelfControlSizeDense) / 2;

// Ink drop color for shelf items.
constexpr SkColor kShelfInkDropBaseColor = SK_ColorWHITE;

// Opacity of the ink drop ripple for shelf items when the ripple is visible.
constexpr float kShelfInkDropVisibleOpacity = 0.2f;

// The foreground color of the icons used in the shelf (launcher,
// notifications, etc).
ASH_EXPORT constexpr SkColor kShelfIconColor = SK_ColorWHITE;

// The alpha value for the shelf background.
ASH_EXPORT constexpr int kShelfTranslucentOverAppList = 51;            // 20%
ASH_EXPORT constexpr int kShelfTranslucentAlpha = 189;                 // 74%
// Using 0xFF causes clipping on the overlay candidate content, which prevent
// HW overlay, probably due to a bug in compositor. Fix it and use 0xFF.
// crbug.com/901538
ASH_EXPORT constexpr int kShelfTranslucentMaximizedWindow = 254;       // ~100%

// The alpha value used to darken a colorized shelf when the shelf is
// translucent.
constexpr int kShelfTranslucentColorDarkenAlpha = 178;

// The alpha value used to darken a colorized shelf when the shelf is opaque.
constexpr int kShelfOpaqueColorDarkenAlpha = 178;

// The distance between the edge of the shelf and the status indicators.
constexpr int kStatusIndicatorOffsetFromShelfEdge = 1;

// Dimensions for hover previews.
constexpr int kShelfTooltipPreviewHeight = 128;
constexpr int kShelfTooltipPreviewMaxWidth = 192;
constexpr float kShelfTooltipPreviewMaxRatio = 1.5;    // = 3/2
constexpr float kShelfTooltipPreviewMinRatio = 0.666;  // = 2/3

class ShelfConstants {
 public:
  // Size of the shelf when visible (height when the shelf is horizontal and
  // width when the shelf is vertical).
  static int shelf_size() {
    return UseNewDenseShelfUi() ? kShelfSizeDense : kShelfSize;
  }

  // Size allocated for each app button on the shelf.
  static int button_size() {
    return UseNewDenseShelfUi() ? kShelfButtonSizeDense : kShelfButtonSize;
  }

  // Size of the space between buttons on the shelf.
  static int button_spacing() { return kShelfButtonSpacing; }

  // Size of the icons within shelf buttons.
  static int button_icon_size() {
    return UseNewDenseShelfUi() ? kShelfButtonIconSizeDense
                                : kShelfButtonIconSize;
  }

  // Size for controls like the home button, back button, etc.
  static int control_size() {
    return UseNewDenseShelfUi() ? kShelfControlSizeDense : kShelfControlSize;
  }

  // The radius of shelf control buttons.
  static int control_border_radius() { return control_size() / 2; }

  static int overflow_button_margin() {
    return UseNewDenseShelfUi() ? kShelfOverflowButtonMarginDense
                                : kShelfOverflowButtonMargin;
  }

  // The distance between the edge of the shelf and the status indicators.
  static int status_indicator_offset_from_edge() {
    return kStatusIndicatorOffsetFromShelfEdge;
  }

  // The distance between the edge of the shelf and the home and back button.
  static int home_button_edge_spacing() {
    return UseNewDenseShelfUi() ? kShelfHomeButtonEdgeSpacingDense
                                : kShelfHomeButtonEdgeSpacing;
  }

 private:
  static bool UseNewDenseShelfUi() {
    static bool use_new_dense_shelf_ui =
        chromeos::switches::ShouldShowShelfDenseClamshell();
    return use_new_dense_shelf_ui;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShelfConstants);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONSTANTS_H_
