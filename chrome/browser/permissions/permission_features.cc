// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

const char kQuietNotificationPromptsActivationParameterName[] = "activation";
const char kQuietNotificationPromptsActivationNever[] = "never";
const char kQuietNotificationPromptsActivationAdaptive[] = "adaptive";
const char kQuietNotificationPromptsActivationAlways[] = "always";

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
QuietNotificationsPromptConfig::Activation
QuietNotificationsPromptConfig::GetActivation() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return Activation::kNever;

  std::string ui_flavor = base::GetFieldTrialParamValueByFeature(
      features::kQuietNotificationPrompts,
      kQuietNotificationPromptsActivationParameterName);
  if (ui_flavor == kQuietNotificationPromptsActivationAlways) {
    return Activation::kAlways;
  } else if (ui_flavor == kQuietNotificationPromptsActivationNever) {
    return Activation::kNever;
  } else if (ui_flavor == kQuietNotificationPromptsActivationAdaptive) {
    return Activation::kAdaptive;
  }
  return Activation::kAdaptive;
}
