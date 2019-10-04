// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/policy_engine.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
bool RealTimePolicyEngine::IsFetchAllowlistEnabled() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupFetchAllowlist);
}

// static
bool RealTimePolicyEngine::IsUrlLookupEnabled() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabled);
}

// static
bool RealTimePolicyEngine::IsUserOptedIn(
    content::BrowserContext* browser_context) {
  PrefService* pref_service = user_prefs::UserPrefs::Get(browser_context);
  return pref_service->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

// static
bool RealTimePolicyEngine::IsEnabledByPolicy(
    content::BrowserContext* browser_context) {
  PrefService* pref_service = user_prefs::UserPrefs::Get(browser_context);
  return pref_service->GetBoolean(prefs::kSafeBrowsingRealTimeLookupEnabled);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookup(
    content::BrowserContext* browser_context) {
  if (!IsFetchAllowlistEnabled())
    return false;

  if (IsEnabledByPolicy(browser_context))
    return true;

  return IsUrlLookupEnabled() && IsUserOptedIn(browser_context);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
    content::ResourceType resource_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.ResourceTypes.Requested",
                            resource_type);
  return resource_type == content::ResourceType::kMainFrame;
}

}  // namespace safe_browsing
