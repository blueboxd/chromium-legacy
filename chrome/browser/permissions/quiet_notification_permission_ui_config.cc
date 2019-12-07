// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

// static
const char QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation[] =
    "enable_adaptive_activation";

// static
const char QuietNotificationPermissionUiConfig::kEnableCrowdDenyTriggering[] =
    "enable_crowd_deny_triggering";

// static
bool QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivation,
      false /* default */);
}

// static
bool QuietNotificationPermissionUiConfig::IsCrowdDenyTriggeringEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableCrowdDenyTriggering,
      false /* default */);
}
