// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_

#include "build/build_config.h"

// Field trial configuration for the quiet notificaiton permission request UI.
class QuietNotificationPermissionUiConfig {
 public:
  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled adaptively after three
  // consecutive prompt denies.
  static const char kEnableAdaptiveActivation[];

  // Name of the boolean variation parameter that determines if the quiet
  // notification permission prompt UI should be enabled as a one-off based on
  // crowd deny data, that is, on sites with a low notification permission grant
  // rate.
  static const char kEnableCrowdDenyTriggering[];

  // Name of the variation parameter that represents the chance that a
  // quiet notifications permission prompt UI triggered by crowd deny will be
  // replaced by the normal UI. This ensures that a small percentage of
  // permission requests still use the normal UI and allows sites to recover
  // from a bad reputation score. Represented as a number in the [0,1] interval.
  // If the quiet UI is enabled in preferences, the quiet UI is always used.
  static const char kCrowdDenyHoldBackChance[];

  // Whether or not adaptive activation is enabled. Adaptive activation means
  // that quiet notifications permission prompts will be turned on after three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationEnabled();

  // Whether or not triggering via crowd deny is enabled. This means that on
  // sites with a low notification permission grant rate, the quiet UI will be
  // shown as a one-off, even when it is not turned on for all sites in prefs.
  static bool IsCrowdDenyTriggeringEnabled();

  // The chance that a quiet notifications permission prompt UI triggered by
  // crowd deny will be replaced by the normal UI. This is per individual
  // permission prompt.
  static double GetCrowdDenyHoldBackChance();
};

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_CONFIG_H_
