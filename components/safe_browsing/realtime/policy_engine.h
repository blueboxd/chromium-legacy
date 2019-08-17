// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_

#include "content/public/common/resource_type.h"

namespace safe_browsing {

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
class RealTimePolicyEngine {
 public:
  RealTimePolicyEngine() = delete;
  ~RealTimePolicyEngine() = delete;

  // Is the feature to sync high confidence allowlist enabled?
  static bool IsFetchAllowlistEnabled();

  // Return true if the feature to enable full URL lookups is enabled and the
  // allowlist fetch is enabled, |resource_type| is kMainFrame.
  static bool CanPerformFullURLLookupForResourceType(
      content::ResourceType resource_type);

  // Return true if the feature to enable full URL lookups is enabled and the
  // allowlist fetch is enabled.
  static bool CanPerformFullURLLookup();

  static bool is_enabled_by_pref() { return is_enabled_by_pref_; }
  friend class SafeBrowsingService;

 private:
  // Is the feature to perform real-time URL lookup enabled?
  static bool IsUrlLookupEnabled();

  // Is user opted-in to the feature?
  static bool IsUserOptedIn();

  // TODO(crbug.com/991394): This is a temporary way of checking whether the
  // real-time lookup is enabled for any active profile. This must be fixed
  // before full launch.
  static bool is_enabled_by_pref_;
  static void SetEnabled(bool is_enabled);

  friend class RealTimePolicyEngineTest;
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
