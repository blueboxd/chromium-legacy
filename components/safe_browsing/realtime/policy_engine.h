// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_

namespace safe_browsing {

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
class RealTimePolicyEngine {
 private:
  RealTimePolicyEngine() = delete;
  ~RealTimePolicyEngine() = delete;

 public:
  // Can the high confidence allowlist be sync'd?
  static bool CanFetchAllowlist();
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
