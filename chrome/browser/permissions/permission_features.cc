// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

const char QuietNotificationsPromptConfig::kEnableAdaptiveActivation[] =
    "enable_adaptive_activation";

QuietNotificationsPromptConfig::UIFlavor
QuietNotificationsPromptConfig::UIFlavorToUse() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return UIFlavor::NONE;

#if defined(OS_ANDROID)
  return UIFlavor::MINI_INFOBAR;
#else   // OS_ANDROID
  return UIFlavor::ANIMATED_ICON;
#endif  // !OS_ANDROID
}

// static
bool QuietNotificationsPromptConfig::IsAdaptiveActivationEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivation,
      false /* default */);
}
