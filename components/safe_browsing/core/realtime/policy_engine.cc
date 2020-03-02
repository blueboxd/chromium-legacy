// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/policy_engine.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/features.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_prefs/user_prefs.h"

#if defined(OS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace safe_browsing {

#if defined(OS_ANDROID)
const int kDefaultMemoryThresholdMb = 4096;
#endif

// static
bool RealTimePolicyEngine::IsUrlLookupEnabled() {
  if (!base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabled))
    return false;
#if defined(OS_ANDROID)
  // On Android, performs real time URL lookup only if
  // |kRealTimeUrlLookupEnabled| is enabled, and system memory is larger than
  // threshold.
  int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
      kRealTimeUrlLookupEnabled, kRealTimeUrlLookupMemoryThresholdMb,
      kDefaultMemoryThresholdMb);
  return base::SysInfo::AmountOfPhysicalMemoryMB() >= memory_threshold_mb;
#else
  return true;
#endif
}

// static
bool RealTimePolicyEngine::IsUserOptedIn(PrefService* pref_service) {
  return pref_service->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

// static
// TODO(crbug.com/1050859): Remove this method.
bool RealTimePolicyEngine::IsEnabledByPolicy() {
  return false;
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookup(PrefService* pref_service,
                                                   bool is_off_the_record) {
  if (is_off_the_record)
    return false;

  if (IsEnabledByPolicy())
    return true;

  return IsUrlLookupEnabled() && IsUserOptedIn(pref_service);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
    PrefService* pref_service,
    bool is_off_the_record) {
  if (!CanPerformFullURLLookup(pref_service, is_off_the_record)) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabledWithToken)) {
    return false;
  }

  // TODO(crbug.com/1041912): Check user sync status.

  return true;
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
    ResourceType resource_type) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.ResourceTypes.Requested",
                            resource_type);
  return resource_type == ResourceType::kMainFrame;
}

}  // namespace safe_browsing
