// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
#define ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "chromeos/constants/chromeos_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// The size delta between the default font and the font size found in tray
// items like labels and buttons.
extern const int kTrayTextFontSizeIncrease;

ASH_EXPORT extern const int kTrayItemSize;

// Extra padding used beside a single icon in the tray area of the shelf.
constexpr int kTrayImageItemPadding = 3;

// The width of the tray menu.
extern const int kTrayMenuWidth;

extern const int kTrayPopupAutoCloseDelayInSeconds;
extern const int kTrayPopupPaddingHorizontal;
extern const int kTrayPopupButtonEndMargin;

// The padding used on the left and right of labels. This applies to all labels
// in the system menu.
extern const int kTrayPopupLabelHorizontalPadding;

// The horizontal padding used to properly lay out a slider in a TriView
// container with a FillLayout (such as a volume notification bubble).
extern const int kTrayPopupSliderHorizontalPadding;

// The minimum/default height of the rows in the system tray menu.
extern const int kTrayPopupItemMinHeight;

// The width used for the first region of the row (which holds an image).
extern const int kTrayPopupItemMinStartWidth;

// The width used for the end region of the row (usually a more arrow).
extern const int kTrayPopupItemMinEndWidth;

// Padding used on right side of labels to keep minimum distance to the next
// item. This applies to all labels in the system menu.
extern const int kTrayPopupLabelRightPadding;

extern const int kTrayRoundedBorderRadius;

// The width of ToggleButton views including any border padding.
extern const int kTrayToggleButtonWidth;

// Constants for the title row.
constexpr int kTitleRowProgressBarHeight = 2;

extern const SkColor kMobileNotConnectedXIconColor;

// Extra padding used to adjust hitting region around tray items.
extern const int kHitRegionPadding;
extern const int kHitRegionPaddingDense;

// Width of lines used to separate menu items (e.g. input method menu).
constexpr int kMenuSeparatorWidth = 1;

// Width of lines used to separate sections of the system tray, for instance
// in tray detailed views.
constexpr int kTraySeparatorWidth = 0;

// The color of the separators used in the system menu.
extern const SkColor kMenuSeparatorColor;

// The size and foreground color of the icons appearing in the material design
// system tray.
constexpr int kTrayIconSize = 16;
extern const SkColor kTrayIconColor;
extern const SkColor kOobeTrayIconColor;
extern const int kTrayIconBackgroundAlpha;

// The padding around network tray icon in dip.
constexpr int kTrayNetworkIconPadding = 2;
constexpr int kUnifiedTrayNetworkIconPadding = 4;

// The size and foreground color of the icons appearing in the material design
// system menu.
extern const int kMenuIconSize;
extern const SkColor kMenuIconColor;
extern const SkColor kMenuIconColorDisabled;
// The size of buttons in the system menu.
ASH_EXPORT extern const int kMenuButtonSize;
// The vertical padding for the system menu separator.
extern const int kMenuSeparatorVerticalPadding;
// The horizontal padding for the system menu separator.
extern const int kMenuExtraMarginFromLeftEdge;
// The visual padding to the left of icons in the system menu.
extern const int kMenuEdgeEffectivePadding;

// The base color used for all ink drops in the system menu.
extern const SkColor kTrayPopupInkDropBaseColor;

// The opacity of the ink drop ripples for all ink drops in the system menu.
extern const float kTrayPopupInkDropRippleOpacity;

// The opacity of the ink drop ripples for all ink highlights in the system
// menu.
extern const float kTrayPopupInkDropHighlightOpacity;

// The inset applied to clickable surfaces in the system menu that do not have
// the ink drop filling the entire bounds.
extern const int kTrayPopupInkDropInset;

// The radius used to draw the corners of the rounded rect style ink drops.
extern const int kTrayPopupInkDropCornerRadius;

// The colors used when --enable-features=SystemTrayUnified flag is enabled.
constexpr SkColor kUnifiedMenuBackgroundColor =
    SkColorSetARGB(0xf2, 0x20, 0x21, 0x24);
constexpr SkColor kUnifiedMenuBackgroundColorWithBlur =
    SkColorSetA(kUnifiedMenuBackgroundColor, 0x99);
constexpr float kUnifiedMenuBackgroundBlur = 30.f;
constexpr SkColor kUnifiedMenuTextColor = SkColorSetRGB(0xf1, 0xf3, 0xf4);
constexpr SkColor kUnifiedMenuIconColor = SkColorSetRGB(0xe8, 0xea, 0xed);
constexpr SkColor kUnifiedMenuSecondaryTextColor =
    SkColorSetA(kUnifiedMenuIconColor, 0xa3);
constexpr SkColor kUnifiedMenuIconColorDisabled =
    SkColorSetRGB(0x5f, 0x63, 0x68);
constexpr SkColor kUnifiedMenuTextColorDisabled =
    SkColorSetRGB(0x5f, 0x63, 0x68);
constexpr SkColor kUnifiedMenuButtonColor =
    SkColorSetA(kUnifiedMenuIconColor, 0x14);
constexpr SkColor kUnifiedMenuSeparatorColor =
    SkColorSetA(kUnifiedMenuIconColor, 0x23);
constexpr SkColor kUnifiedMenuButtonColorActive =
    SkColorSetRGB(0x25, 0x81, 0xdf);
constexpr SkColor kUnifiedMenuButtonColorDisabled =
    SkColorSetA(kUnifiedMenuButtonColor, 0xa);
constexpr SkColor kUnifiedNotificationSeparatorColor =
    SkColorSetRGB(0xdf, 0xe0, 0xe0);
constexpr SkColor kUnifiedFeaturePodHoverColor =
    SkColorSetRGB(0xff, 0xff, 0xff);
constexpr SkColor kUnifiedRecordingIconColor = gfx::kGoogleRedDark600;
constexpr SkColor kUnifiedManagedDeviceIconColor =
    SkColorSetRGB(0x9a, 0xa0, 0xa6);

constexpr gfx::Insets kUnifiedMenuItemPadding(0, 16, 16, 16);
constexpr gfx::Insets kUnifiedSystemInfoViewPadding(4, 16, 16, 16);
constexpr gfx::Insets kUnifiedManagedDeviceViewPadding(4, 19, 4, 16);
constexpr gfx::Insets kUnifiedSliderRowPadding(0, 12, 8, 16);
constexpr gfx::Insets kUnifiedSliderBubblePadding(12, 0, 4, 0);
constexpr gfx::Insets kUnifiedSliderPadding(0, 16);

constexpr int kUnifiedSliderViewSpacing = 12;
constexpr int kUnifiedMenuPadding = 8;
constexpr int kUnifiedNotificationCenterSpacing = 16;
constexpr int kUnifiedTrayIconSize = 20;
constexpr int kUnifiedTraySpacingBetweenIcons = 6;
constexpr int kUnifiedTrayBatteryWidth = 10;
constexpr int kUnifiedTrayCornerRadius = 20;
constexpr int kUnifiedTrayContentPadding = 12;
constexpr int kUnifiedTopShortcutSpacing = 16;
constexpr int kUnifiedNotificationHiddenLineHeight = 20;
constexpr int kUnifiedTopShortcutContainerTopPadding = 12;
constexpr int kUnifiedNotificationMinimumHeight = 40;
constexpr gfx::Insets kUnifiedTopShortcutPadding(0, 16);
constexpr gfx::Insets kUnifiedNotificationHiddenPadding(6, 16);
constexpr gfx::Insets kUnifiedCircularButtonFocusPadding(4);
constexpr int kStackingNotificationCounterHeight = 32;
constexpr gfx::Insets kStackingNotificationClearAllButtonPadding(8, 16);
constexpr SkColor kStackingNotificationCounterBorderColor =
    SkColorSetRGB(0xe0, 0xe0, 0xe0);
constexpr SkColor kStackingNotificationCounterLabelColor =
    SkColorSetRGB(0x5f, 0x64, 0x68);

// Size of an icon drawn inside top shortcut buttons.
// A dark disc with |kTrayItemSize| diameter is drawn in the background.
constexpr int kTrayTopShortcutButtonIconSize = 20;

constexpr int kUnifiedManagedDeviceSpacing = 4;
constexpr int kUnifiedSystemInfoHeight = 16;
constexpr int kUnifiedSystemInfoSpacing = 8;
constexpr int kUnifiedSystemInfoSeparatorColor =
    SkColorSetA(SK_ColorWHITE, 0x24);
constexpr gfx::Insets kUnifiedSystemInfoDateViewPadding(3);

// Constants used in FeaturePodsView of UnifiedSystemTray.
constexpr gfx::Size kUnifiedFeaturePodIconSize(48, 48);
constexpr gfx::Size kUnifiedFeaturePodSize(112, 88);
constexpr gfx::Size kUnifiedFeaturePodCollapsedSize(48, 48);
constexpr gfx::Insets kUnifiedFeaturePodIconPadding(4);
constexpr gfx::Insets kUnifiedFeaturePodHoverPadding(2);
constexpr int kUnifiedFeaturePodVectorIconSize = 24;
constexpr int kUnifiedFeaturePodLabelWidth = 80;
constexpr int kUnifiedFeaturePodSpacing = 6;
constexpr int kUnifiedFeaturePodHoverRadius = 4;
constexpr int kUnifiedFeaturePodVerticalPadding = 28;
constexpr int kUnifiedFeaturePodTopPadding = 24;
constexpr int kUnifiedFeaturePodBottomPadding = 5;
constexpr int kUnifiedFeaturePodHorizontalSidePadding = 12;
constexpr int kUnifiedFeaturePodHorizontalMiddlePadding = 0;
constexpr int kUnifiedFeaturePodCollapsedVerticalPadding = 12;
constexpr int kUnifiedFeaturePodCollapsedHorizontalPadding = 24;
constexpr int kUnifiedFeaturePodArrowSpacing = 4;
constexpr int kUnifiedFeaturePodItemsInRow = 3;
constexpr int kUnifiedFeaturePodMaxRows = 3;
constexpr int kUnifiedFeaturePodMinRows = 1;
constexpr int kUnifiedFeaturePodMaxItemsInCollapsed = 5;
constexpr int kUnifiedFeaturePodsPageSpacing = 48;
constexpr int kUnifiedNotificationSeparatorThickness = 1;

// Constants used in system tray page transition animations (ms).
constexpr int kUnifiedSystemTrayPageTransitionDurationMs = 250;
constexpr int kUnifiedSystemTrayOverScrollPageTransitionDurationMs = 50;
constexpr double kCollapseThreshold = 0.3;

// Constants used in PageIndicatorView of UnifiedSystemTray.
constexpr int kUnifiedPageIndicatorButtonRadius = 3;
constexpr SkColor kUnifiedPageIndicatorButtonColor =
    SkColorSetRGB(0xF1, 0xF3, 0xF4);
constexpr SkColor kUnifiedPageIndicatorButtonAlpha = 0x6E;
constexpr SkColor kUnifiedPageIndicatorButtonInkDropColor =
    SkColorSetRGB(0xFF, 0xFF, 0xFF);

// Separators between multiple users are shorter than the full width.
constexpr int kUnifiedUserChooserSeparatorSideMargin = 64;
// Additional gap above and below the longer separator between user list and
// "Sign in another user..." button.
constexpr int kUnifiedUserChooserLargeSeparatorVerticalSpacing = 8;

constexpr int kUnifiedUserChooserRowHeight = 64;

// Gap between the buttons on the top shortcut row, other than the
// expand/collapse button.
constexpr int kUnifiedTopShortcutButtonDefaultSpacing = 16;
constexpr int kUnifiedTopShortcutButtonMinSpacing = 4;

// Constants used in the title row of a detailed view in UnifiedSystemTray.
constexpr gfx::Insets kUnifiedDetailedViewTitlePadding(0, 0, 0, 16);
constexpr int kUnifiedDetailedViewTitleRowHeight = 64;

class TrayConstants {
 public:
  static int hit_region_padding() {
    return UseNewDenseShelfUi() ? kHitRegionPaddingDense : kHitRegionPadding;
  }

 private:
  static bool UseNewDenseShelfUi() {
    static bool use_new_dense_shelf_ui =
        chromeos::switches::ShouldShowShelfDenseClamshell();
    return use_new_dense_shelf_ui;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(TrayConstants);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_CONSTANTS_H_
