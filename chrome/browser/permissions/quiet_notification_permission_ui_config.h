// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_

#include "build/build_config.h"

// Field trial configuration for the quiet notificaiton permission request UI.
class QuietNotificationPermissionUiConfig {
 public:
  enum UiFlavor {
    NONE,
#if defined(OS_ANDROID)
    QUIET_NOTIFICATION,
    HEADS_UP_NOTIFICATION,
    MINI_INFOBAR,
#else   // OS_ANDROID
    STATIC_ICON,
    ANIMATED_ICON,
#endif  // OS_ANDROID
  };

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled adaptively after three
  // consecutive prompt denies.
  static const char kEnableAdaptiveActivation[];

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled as a one-off based on
  // crowd deny data, that is, on sites with a low notification permission grant
  // rate.
  static const char kEnableCrowdDenyTriggering[];

  // Which flavor of the quiet UI to use.
  //
  // Deprecated: The UI flavor is now hardcoded on each platform and no longer
  // controlled by field trials.
  static UiFlavor UiFlavorToUse();

  // Whether or not adaptive activation is enabled. Adaptive activation means
  // that quiet notifications permission prompts will be turned on after three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationEnabled();

  // Whether or not triggering via crowd deny is enabled. This means that on
  // sites with a low notification permission grant rate, the quiet UI will be
  // shown as a one-off, even when it is not turned on for all sites in prefs.
  static bool IsCrowdDenyTriggeringEnabled();
};

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
