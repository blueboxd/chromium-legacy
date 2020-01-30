// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_switches.h"

namespace ash {
namespace features {

const base::Feature kAllowAmbientEQ{"AllowAmbientEQ",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutoNightLight{"AutoNightLight",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisplayChangeModal{"DisplayChangeModal",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDockedMagnifier{"DockedMagnifier",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDragToSnapInClamshellMode{
    "DragToSnapInClamshellMode", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableOverviewRoundedCorners{
    "EnableOverviewRoundedCorners", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kLockScreenNotifications{"LockScreenNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenInlineReply{"LockScreenInlineReply",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenHideSensitiveNotificationsSupport{
    "LockScreenHideSensitiveNotificationsSupport",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLockScreenMediaControls{"LockScreenMediaControls",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHideArcMediaNotifications{
    "HideArcMediaNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kManagedDeviceUIRedesign{"ManagedDeviceUIRedesign",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMediaSessionNotification{"MediaSessionNotification",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMultiDisplayOverviewAndSplitView{
    "MultiDisplayOverviewAndSplitView", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNewOverviewLayout{"NewOverviewLayout",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNightLight{"NightLight", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNotificationExpansionAnimation{
    "NotificationExpansionAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNotificationScrollBar{"NotificationScrollBar",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPipRoundedCorners{"PipRoundedCorners",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReduceDisplayNotifications{
    "ReduceDisplayNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSeparateNetworkIcons{"SeparateNetworkIcons",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrilinearFiltering{"TrilinearFiltering",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUnlockWithExternalBinary{
    "UnlockWithExternalBinary", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kViewsLogin{"ViewsLogin", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kVirtualDesks{"VirtualDesks",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUseBluetoothSystemInAsh{"UseBluetoothSystemInAsh",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSupervisedUserDeprecationNotice{
    "SupervisedUserDeprecationNotice", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSwapSideVolumeButtonsForOrientation{
    "SwapSideVolumeButtonsForOrientation", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUnifiedMessageCenterRefactor{
    "UnifiedMessageCenterRefactor", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableBackgroundBlur{"EnableBackgroundBlur",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSwipingFromLeftEdgeToGoBack{
    "SwipingFromLeftEdgeToGoBack", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDragFromShelfToHomeOrOverview{
    "DragFromShelfToHomeOrOverview", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHideShelfControlsInTabletMode{
    "HideShelfControlsInTabletMode", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDragWindowFromTop{"DragWindowFromTop",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAllowAmbientEQEnabled() {
  return base::FeatureList::IsEnabled(kAllowAmbientEQ);
}

bool IsAutoNightLightEnabled() {
  return base::FeatureList::IsEnabled(kAutoNightLight);
}

bool IsHideArcMediaNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kMediaSessionNotification) &&
         base::FeatureList::IsEnabled(kHideArcMediaNotifications);
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

bool IsLockScreenInlineReplyEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenInlineReply);
}

bool IsLockScreenHideSensitiveNotificationsSupported() {
  return base::FeatureList::IsEnabled(
      kLockScreenHideSensitiveNotificationsSupport);
}

bool IsManagedDeviceUIRedesignEnabled() {
  return base::FeatureList::IsEnabled(kManagedDeviceUIRedesign);
}

bool IsNotificationExpansionAnimationEnabled() {
  return base::FeatureList::IsEnabled(kNotificationExpansionAnimation);
}

bool IsNotificationScrollBarEnabled() {
  return base::FeatureList::IsEnabled(kNotificationScrollBar);
}

bool IsPipRoundedCornersEnabled() {
  return base::FeatureList::IsEnabled(kPipRoundedCorners);
}

bool IsSeparateNetworkIconsEnabled() {
  return base::FeatureList::IsEnabled(kSeparateNetworkIcons);
}

bool IsTrilinearFilteringEnabled() {
  static bool use_trilinear_filtering =
      base::FeatureList::IsEnabled(kTrilinearFiltering);
  return use_trilinear_filtering;
}

bool IsVirtualDesksEnabled() {
  return base::FeatureList::IsEnabled(kVirtualDesks);
}

bool IsViewsLoginEnabled() {
  // Always show webui login if --show-webui-login is present, which is passed
  // by session manager for automatic recovery. Otherwise, only show views
  // login if the feature is enabled.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kShowWebUiLogin) &&
         base::FeatureList::IsEnabled(kViewsLogin);
}

bool IsSupervisedUserDeprecationNoticeEnabled() {
  return base::FeatureList::IsEnabled(kSupervisedUserDeprecationNotice);
}

bool IsSwapSideVolumeButtonsForOrientationEnabled() {
  return base::FeatureList::IsEnabled(kSwapSideVolumeButtonsForOrientation);
}

bool IsUnifiedMessageCenterRefactorEnabled() {
  return base::FeatureList::IsEnabled(kUnifiedMessageCenterRefactor) ||
         chromeos::switches::ShouldShowShelfHotseat();
}

bool IsBackgroundBlurEnabled() {
  bool enabled_by_feature_flag =
      base::FeatureList::IsEnabled(kEnableBackgroundBlur);
#if defined(ARCH_CPU_ARM_FAMILY)
  // Enable background blur on Mali when GPU rasterization is enabled.
  // See crbug.com/996858 for the condition.
  return enabled_by_feature_flag &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kAshEnableTabletMode);
#else
  return enabled_by_feature_flag;
#endif
}

bool IsSwipingFromLeftEdgeToGoBackEnabled() {
  // The kSwipingFromLeftEdgeToGoBack feature is only enabled on the devices
  // that have hotseat enabled (i.e., on Krane and on Dogfood devices) in M80.
  // See crbug.com/1030122 for details.
  return base::FeatureList::IsEnabled(kSwipingFromLeftEdgeToGoBack) ||
         chromeos::switches::ShouldShowShelfHotseat();
}

bool IsDragFromShelfToHomeOrOverviewEnabled() {
  // The kDragFromShelfToHomeOrOverview feature is only enabled on the devices
  // that have hotseat enabled (i.e., on Krane and on Dogfood devices) in M80.
  // See crbug.com/1029991 for details.
  return base::FeatureList::IsEnabled(kDragFromShelfToHomeOrOverview) ||
         chromeos::switches::ShouldShowShelfHotseat();
}

bool IsReduceDisplayNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kReduceDisplayNotifications);
}

bool IsHideShelfControlsInTabletModeEnabled() {
  return base::FeatureList::IsEnabled(kHideShelfControlsInTabletMode) &&
         IsDragFromShelfToHomeOrOverviewEnabled();
}

bool IsDisplayChangeModalEnabled() {
  return base::FeatureList::IsEnabled(kDisplayChangeModal);
}

}  // namespace features
}  // namespace ash
