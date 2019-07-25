// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/policy_engine.h"

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"

namespace safe_browsing {

RealTimePolicyEngine::RealTimePolicyEngine(PrefService* pref_service)
    : pref_service_(pref_service) {}

RealTimePolicyEngine::~RealTimePolicyEngine() {}

// static
bool RealTimePolicyEngine::CanFetchAllowlist() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupFetchAllowlist);
}

// static
bool RealTimePolicyEngine::CanPerformFullURLLookup() {
  // TODO(vakh): This should also take into account whether the user is eligible
  // for this service (see "Target Users" in the design doc).
  return CanFetchAllowlist() &&
         (base::FeatureList::IsEnabled(kRealTimeUrlLookupEnabled) ||
          pref_service_->GetBoolean(prefs::kSafeBrowsingRealTimeLookupEnabled));
}

}  // namespace safe_browsing
