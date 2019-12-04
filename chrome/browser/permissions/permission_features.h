// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_

#include "build/build_config.h"

class QuietNotificationsPromptConfig {
 public:
  enum UIFlavor {
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

  static UIFlavor UIFlavorToUse();

  // Whether or not adaptive activation is enabled. Adaptive activation means
  // that quiet notifications permission prompts will be turned on after three
  // consecutive prompt denies.
  static bool IsAdaptiveActivationEnabled();
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_
