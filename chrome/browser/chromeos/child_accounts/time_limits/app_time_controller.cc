// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

namespace chromeos {
namespace app_time {

// static
bool AppTimeController::ArePerAppTimeLimitsEnabled() {
  return base::FeatureList::IsEnabled(features::kPerAppTimeLimits);
}

AppTimeController::AppTimeController(Profile* profile)
    : app_service_wrapper_(std::make_unique<AppServiceWrapper>(profile)),
      app_registry_(
          std::make_unique<AppActivityRegistry>(app_service_wrapper_.get())) {
  DCHECK(profile);

  if (WebTimeLimitEnforcer::IsEnabled())
    web_time_enforcer_ = std::make_unique<WebTimeLimitEnforcer>();
}

AppTimeController::~AppTimeController() = default;

}  // namespace app_time
}  // namespace chromeos
