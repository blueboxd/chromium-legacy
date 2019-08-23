// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_

#include "build/build_config.h"

#if defined(OS_ANDROID)

extern const char kQuietNotificationPromptsUIFlavourParameterName[];
extern const char kQuietNotificationPromptsHeadsUpNotification[];
extern const char kQuietNotificationPromptsMiniInfobar[];

class QuietNotificationsPromptConfig {
 public:
  enum UIFlavor {
    NONE,
    QUIET_NOTIFICATION,
    HEADS_UP_NOITIFCATION,
    MINI_INFOBAR,
  };

  static UIFlavor UIFlavorToUse();
};

#endif  // OS_ANDROID

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_FEATURES_H_
