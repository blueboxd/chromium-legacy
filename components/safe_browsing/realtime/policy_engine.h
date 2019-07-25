// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_

class PrefService;

namespace safe_browsing {

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
class RealTimePolicyEngine {
 public:
  explicit RealTimePolicyEngine(PrefService* pref_service);
  ~RealTimePolicyEngine();

  // Can the high confidence allowlist be sync'd?
  static bool CanFetchAllowlist();

  // Return true if the feature to enable full URL lookups is enabled and the
  // allowlist fetch is enabled.
  bool CanPerformFullURLLookup();

 private:
  // Does not indicate ownership of |pref_service_|. The owner of the
  // RealTimePolicyEngine is responsible for ensuring |pref_service_| outlasts
  // this.
  PrefService* pref_service_;
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
