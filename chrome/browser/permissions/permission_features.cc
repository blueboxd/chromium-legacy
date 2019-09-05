// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

#if defined(OS_ANDROID)

// Keep in sync with "PermissionFieldTrial.java"

const char kQuietNotificationPromptsUIFlavourParameterName[] = "ui_flavour";
const char kQuietNotificationPromptsHeadsUpNotification[] =
    "heads_up_notification";
const char kQuietNotificationPromptsMiniInfobar[] = "mini_infobar";

QuietNotificationsPromptConfig::UIFlavor
QuietNotificationsPromptConfig::UIFlavorToUse() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return UIFlavor::NONE;
  std::string ui_flavor = base::GetFieldTrialParamValueByFeature(
      features::kQuietNotificationPrompts,
      kQuietNotificationPromptsUIFlavourParameterName);
  if (ui_flavor == kQuietNotificationPromptsHeadsUpNotification) {
    return UIFlavor::HEADS_UP_NOITIFCATION;
  } else if (ui_flavor == kQuietNotificationPromptsMiniInfobar) {
    return UIFlavor::MINI_INFOBAR;
  } else {
    return UIFlavor::QUIET_NOTIFICATION;
  }
}

#endif  // OS_ANDROID
