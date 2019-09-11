// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_

#include "build/build_config.h"


extern const char kQuietNotificationPromptsUIFlavourParameterName[];

#if defined(OS_ANDROID)
extern const char kQuietNotificationPromptsHeadsUpNotification[];
extern const char kQuietNotificationPromptsMiniInfobar[];
#else   // OS_ANDROID
extern const char kQuietNotificationPromptsStaticIcon[];
extern const char kQuietNotificationPromptsAnimatedIcon[];
#endif  // OS_ANDROID

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
  static UIFlavor UIFlavorToUse();
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_
