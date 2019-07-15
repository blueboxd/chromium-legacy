// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_CONSTANTS_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

// Accessibility identifier prefix of a grid cell. To reference a specific cell,
// concatenate |kGridCellIdentifierPrefix| with the index of the cell. For
// example, [NSString stringWithFormat:@"%@%d", kGridCellIdentifierPrefix,
// index].
extern NSString* const kGridCellIdentifierPrefix;

// Accessibility identifier for the close button in a grid cell.
extern NSString* const kGridCellCloseButtonIdentifier;

// All kxxxColor constants are RGB values stored in a Hex integer. These will be
// converted into UIColors using the UIColorFromRGB() function, from
// uikit_ui_util.h

// Grid styling.
extern const int kGridBackgroundColor;

// GridLayout.
// Extra-small screens require a slightly different layout configuration (e.g.,
// margins) even though they may be categorized into the same size class as
// larger screens. These screens are determined to have a "limited width" in
// their size class by the definition below. The first size class refers to the
// horizontal; the second to the vertical.
extern const CGFloat kGridLayoutCompactCompactLimitedWidth;
extern const CGFloat kGridLayoutCompactRegularLimitedWidth;
// Insets for size classes. The first refers to the horizontal size class; the
// second to the vertical.
extern const UIEdgeInsets kGridLayoutInsetsCompactCompact;
extern const UIEdgeInsets kGridLayoutInsetsCompactCompactLimitedWidth;
extern const UIEdgeInsets kGridLayoutInsetsCompactRegular;
extern const UIEdgeInsets kGridLayoutInsetsCompactRegularLimitedWidth;
extern const UIEdgeInsets kGridLayoutInsetsRegularCompact;
extern const UIEdgeInsets kGridLayoutInsetsRegularRegular;
// Minimum line spacing for size classes. The first refers to the horizontal
// size class; the second to the vertical.
extern const CGFloat kGridLayoutLineSpacingCompactCompact;
extern const CGFloat kGridLayoutLineSpacingCompactCompactLimitedWidth;
extern const CGFloat kGridLayoutLineSpacingCompactRegular;
extern const CGFloat kGridLayoutLineSpacingCompactRegularLimitedWidth;
extern const CGFloat kGridLayoutLineSpacingRegularCompact;
extern const CGFloat kGridLayoutLineSpacingRegularRegular;

// GridReorderingLayout.
// Opacity for cells that aren't being moved.
extern const CGFloat kReorderingInactiveCellOpacity;
// Scale for the cell that is being moved.
extern const CGFloat kReorderingActiveCellScale;

// GridCell styling.
// Common colors.
extern const int kGridCellIconBackgroundColor;
extern const int kGridCellSnapshotBackgroundColor;
// Light theme colors.
extern const int kGridLightThemeCellTitleColor;
extern const int kGridLightThemeCellHeaderColor;
extern const int kGridLightThemeCellSelectionColor;
extern const int kGridLightThemeCellCloseButtonTintColor;
// Dark theme colors.
extern const int kGridDarkThemeCellTitleColor;
extern const int kGridDarkThemeCellHeaderColor;
extern const int kGridDarkThemeCellSelectionColor;
extern const int kGridDarkThemeCellCloseButtonTintColor;
extern const CGFloat kGridDarkThemeCellHighlightColorAlpha;
extern const int kGridDarkThemeCellSeparatorColor;
// TODO (crbug.com/981889): remove with iOS 12.
// Extra dark theme colors until iOS 12 gets removed.
extern const int kGridDarkThemeCellDetailColor;
extern const CGFloat kGridDarkThemeCellDetailAlpha;
extern const int kGridDarkThemeCellTintColor;
extern const int kGridDarkThemeCellSolidButtonTextColor;

// GridCell dimensions.
extern const CGSize kGridCellSizeSmall;
extern const CGSize kGridCellSizeMedium;
extern const CGSize kGridCellSizeLarge;
extern const CGSize kGridCellSizeAccessibility;
extern const CGFloat kGridCellCornerRadius;
extern const CGFloat kGridCellIconCornerRadius;
// The cell header contains the icon, title, and close button.
extern const CGFloat kGridCellHeaderHeight;
extern const CGFloat kGridCellHeaderAccessibilityHeight;
extern const CGFloat kGridCellHeaderLeadingInset;
extern const CGFloat kGridCellCloseTapTargetWidthHeight;
extern const CGFloat kGridCellCloseButtonContentInset;
extern const CGFloat kGridCellTitleLabelContentInset;
extern const CGFloat kGridCellIconDiameter;
extern const CGFloat kGridCellSelectionRingGapWidth;
extern const CGFloat kGridCellSelectionRingTintWidth;

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_CONSTANTS_H_
