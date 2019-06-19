// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/policy_engine.h"

#include "base/feature_list.h"
#include "components/safe_browsing/features.h"

namespace safe_browsing {

// static
bool RealTimePolicyEngine::CanFetchAllowlist() {
  return base::FeatureList::IsEnabled(kRealTimeUrlLookupFetchAllowlist);
}

}  // namespace safe_browsing
