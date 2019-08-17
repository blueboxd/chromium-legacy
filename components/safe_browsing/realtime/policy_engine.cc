// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/policy_engine.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"

namespace safe_browsing {

bool RealTimePolicyEngine::is_enabled_by_pref_ = false;

// static
bool RealTimePolicyEngine::IsFetchAllowlistEnabled() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupFetchAllowlist);
}

// static
bool RealTimePolicyEngine::IsUrlLookupEnabled() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabled);
}

// static
bool RealTimePolicyEngine::IsUserOptedIn() {
  // TODO(crbug.com/991394): Implement this soon. For now, disabled.
  return false;
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookup() {
  // TODO(crbug.com/991394): This should take into account whether the user is
  // eligible for this service (see "Target Users" in the design doc).

  if (!IsFetchAllowlistEnabled())
    return false;

  if (is_enabled_by_pref())
    return true;

  return IsUrlLookupEnabled() && IsUserOptedIn();
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
    content::ResourceType resource_type) {
  if (!CanPerformFullURLLookup())
    return false;

  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.ResourceTypes.Requested",
                            resource_type);
  return resource_type == content::ResourceType::kMainFrame;
}

// static
void RealTimePolicyEngine::SetEnabled(bool is_enabled) {
  is_enabled_by_pref_ = is_enabled;
}

}  // namespace safe_browsing
