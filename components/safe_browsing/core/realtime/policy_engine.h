// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_REALTIME_POLICY_ENGINE_H_

#include "build/build_config.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {

enum class ResourceType;

#if defined(OS_ANDROID)
// A parameter controlled by finch experiment.
// On Android, performs real time URL lookup only if |kRealTimeUrlLookupEnabled|
// is enabled, and system memory is larger than threshold.
const char kRealTimeUrlLookupMemoryThresholdMb[] =
    "SafeBrowsingRealTimeUrlLookupMemoryThresholdMb";
#endif

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
// TODO(crbug.com/1050859): To make this class build in IOS, remove
// browser_context dependency in this class, and replace it with pref_service
// and simple_factory_key.
class RealTimePolicyEngine {
 public:
  RealTimePolicyEngine() = delete;
  ~RealTimePolicyEngine() = delete;

  // Return true if full URL lookups are enabled for |resource_type|.
  static bool CanPerformFullURLLookupForResourceType(
      ResourceType resource_type);

  // Return true if the feature to enable full URL lookups is enabled and the
  // allowlist fetch is enabled for the profile represented by
  // |browser_context|.
  static bool CanPerformFullURLLookup(content::BrowserContext* browser_context);

  // Return true if the OAuth token should be associated with the URL lookup
  // pings.
  static bool CanPerformFullURLLookupWithToken(
      content::BrowserContext* browser_context);

  friend class SafeBrowsingService;
  friend class SafeBrowsingUIHandler;

 private:
  // Is the feature to perform real-time URL lookup enabled?
  static bool IsUrlLookupEnabled();

  // Is user opted-in to the feature?
  static bool IsUserOptedIn(content::BrowserContext* browser_context);

  // Is the feature enabled due to enterprise policy?
  static bool IsEnabledByPolicy(content::BrowserContext* browser_context);

  friend class RealTimePolicyEngineTest;
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_REALTIME_POLICY_ENGINE_H_
