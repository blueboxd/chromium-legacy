// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

// Keep in sync with "PermissionFieldTrial.java"

const char kQuietNotificationPromptsUIFlavourParameterName[] = "ui_flavour";

#if defined(OS_ANDROID)
const char kQuietNotificationPromptsHeadsUpNotification[] =
    "heads_up_notification";
const char kQuietNotificationPromptsMiniInfobar[] = "mini_infobar";
#else   // OS_ANDROID
const char kQuietNotificationPromptsStaticIcon[] = "static_icon";
const char kQuietNotificationPromptsAnimatedIcon[] = "animated_icon";
#endif  // !OS_ANDROID

QuietNotificationsPromptConfig::UIFlavor
QuietNotificationsPromptConfig::UIFlavorToUse() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return UIFlavor::NONE;

  std::string ui_flavor = base::GetFieldTrialParamValueByFeature(
      features::kQuietNotificationPrompts,
      kQuietNotificationPromptsUIFlavourParameterName);

#if defined(OS_ANDROID)
  if (ui_flavor == kQuietNotificationPromptsHeadsUpNotification) {
    return UIFlavor::HEADS_UP_NOTIFICATION;
  } else if (ui_flavor == kQuietNotificationPromptsMiniInfobar) {
    return UIFlavor::MINI_INFOBAR;
  } else {
    return UIFlavor::QUIET_NOTIFICATION;
  }
#else   // OS_ANDROID
  if (ui_flavor == kQuietNotificationPromptsStaticIcon) {
    return UIFlavor::STATIC_ICON;
  } else if (ui_flavor == kQuietNotificationPromptsAnimatedIcon) {
    return UIFlavor::ANIMATED_ICON;
  } else {
    return UIFlavor::STATIC_ICON;
  }
#endif  // !OS_ANDROID
}
