// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include "base/logging.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace {

using content::NavigationHandle;
using content::NavigationThrottle;

bool IsWellKnownChangePasswordUrl(const GURL& url) {
  return url.is_valid() && url.has_path() &&
         (url.PathForRequest() == "/.well-known/change-password" ||
          url.PathForRequest() == "/.well-known/change-password/");
}

}  // namespace

// static
std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  const GURL& url = handle->GetURL();
  // The order is important. We have to check if it as a well-known change
  // password url first. We should only check the feature flag when the feature
  // would be used. Otherwise the we would not see a difference between control
  // and experiment groups on the dashboards.
  if (IsWellKnownChangePasswordUrl(url) &&
      base::FeatureList::IsEnabled(
          password_manager::features::kWellKnownChangePassword)) {
    return base::WrapUnique(
        new WellKnownChangePasswordNavigationThrottle(handle));
  }
  return nullptr;
}

WellKnownChangePasswordNavigationThrottle::
    WellKnownChangePasswordNavigationThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

WellKnownChangePasswordNavigationThrottle::
    ~WellKnownChangePasswordNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillStartRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillFailRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
WellKnownChangePasswordNavigationThrottle::WillProcessResponse() {
  return NavigationThrottle::PROCEED;
}

const char* WellKnownChangePasswordNavigationThrottle::GetNameForLogging() {
  return "WellKnownChangePasswordNavigationThrottle";
}
