// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_FEATURES_H_
#define ASH_PUBLIC_CPP_ASH_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"

namespace ash {
namespace features {

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
ASH_PUBLIC_EXPORT extern const base::Feature kDockedMagnifier;

// Enables dragging and snapping an overview window in clamshell mode.
// TODO(crbug.com/890029): Remove this when the feature is fully launched.
ASH_PUBLIC_EXPORT extern const base::Feature kDragToSnapInClamshellMode;

// Enables rounded corners in overview mode for testing.
// TODO(crbug.com/903486): Remove this when new rounded corners implementation
// has landed.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableOverviewRoundedCorners;

// Enables notifications on the lock screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenNotifications;

// Enables inline reply on notifications on the lock screen.
// This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenInlineReply;

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature
    kLockScreenHideSensitiveNotificationsSupport;

// Enables user preference to control media keys on the lock screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenMediaKeys;

// Enables lock screen media controls UI.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenMediaControls;

// Enables hiding of ARC media notifications. If this is enabled, all ARC
// notifications that are of the media type will not be shown. This
// is because they will be replaced by native media session notifications.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
ASH_PUBLIC_EXPORT extern const base::Feature kHideArcMediaNotifications;

// Enables the media session notification. If this is enabled, we will show
// a notification that shows the currently playing media with controls.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
ASH_PUBLIC_EXPORT extern const base::Feature kMediaSessionNotification;

// Enables multi-display support for overview and split view.
// TODO(crbug.com/952461): Remove this when the feature is fully launched.
ASH_PUBLIC_EXPORT extern const base::Feature kMultiDisplayOverviewAndSplitView;

// Enables the redesigned managed device info UI in the system tray.
ASH_PUBLIC_EXPORT extern const base::Feature kManagedDeviceUIRedesign;

// Enables the Night Light feature.
ASH_PUBLIC_EXPORT extern const base::Feature kNightLight;

// Enabled notification expansion animation.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationExpansionAnimation;

// Enables notification scroll bar in UnifiedSystemTray.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationScrollBar;

// Enables rounded corners for the Picture-in-picture window.
ASH_PUBLIC_EXPORT extern const base::Feature kPipRoundedCorners;

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
ASH_PUBLIC_EXPORT extern const base::Feature kSeparateNetworkIcons;

// Enables trilinear filtering.
ASH_PUBLIC_EXPORT extern const base::Feature kTrilinearFiltering;

// Enables running an external binary which provides lock screen authentication.
ASH_PUBLIC_EXPORT extern const base::Feature kUnlockWithExternalBinary;

// Enables the KioskNextShell feature.
ASH_PUBLIC_EXPORT extern const base::Feature kKioskNextShell;

// Enables views login.
ASH_PUBLIC_EXPORT extern const base::Feature kViewsLogin;

// Enables the Virtual Desks feature.
ASH_PUBLIC_EXPORT extern const base::Feature kVirtualDesks;

// Enables using the BluetoothSystem Mojo interface for Bluetooth operations.
ASH_PUBLIC_EXPORT extern const base::Feature kUseBluetoothSystemInAsh;

// Enables the Supervised User Deprecation notices.
ASH_PUBLIC_EXPORT extern const base::Feature kSupervisedUserDeprecationNotice;

// Enables pagination for feature pod buttons in the system tray
ASH_PUBLIC_EXPORT extern const base::Feature kSystemTrayFeaturePodsPagination;

// Enables side volume button control based on screen orientation feature.
// TODO(https://crbug.com/937907): Remove this after the feature is fully
// launched.
ASH_PUBLIC_EXPORT extern const base::Feature
    kSwapSideVolumeButtonsForOrientation;

ASH_PUBLIC_EXPORT bool IsHideArcMediaNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerAppEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsManagedDeviceUIRedesignEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenInlineReplyEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenHideSensitiveNotificationsSupported();

ASH_PUBLIC_EXPORT bool IsNotificationExpansionAnimationEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationScrollBarEnabled();

ASH_PUBLIC_EXPORT bool IsPipRoundedCornersEnabled();

ASH_PUBLIC_EXPORT bool IsSeparateNetworkIconsEnabled();

ASH_PUBLIC_EXPORT bool IsTrilinearFilteringEnabled();

ASH_PUBLIC_EXPORT bool IsViewsLoginEnabled();

ASH_PUBLIC_EXPORT bool IsVirtualDesksEnabled();

ASH_PUBLIC_EXPORT bool IsSupervisedUserDeprecationNoticeEnabled();

ASH_PUBLIC_EXPORT bool IsSystemTrayFeaturePodsPaginationEnabled();

ASH_PUBLIC_EXPORT bool IsSwapSideVolumeButtonsForOrientationEnabled();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
