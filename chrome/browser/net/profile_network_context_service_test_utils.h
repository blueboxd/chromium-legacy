// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"

// Indicates the state of the feature flags
// |kEnableAmbientAuthenticationInIncognito| and
// |kEnableAmbientAuthenticationInGuestSession|
enum class AmbientAuthenticationFeatureState {
  GUEST_OFF_INCOGNITO_OFF = 0,  // 00
  GUEST_OFF_INCOGNITO_ON = 1,   // 01
  GUEST_ON_INCOGNITO_OFF = 2,   // 10
  GUEST_ON_INCOGNITO_ON = 3,    // 11
};

class AmbientAuthenticationTestHelper {
 public:
  AmbientAuthenticationTestHelper() = default;

  static bool IsAmbientAuthAllowedForProfile(Profile* profile);

  static bool IsIncognitoAllowedInFeature(
      const AmbientAuthenticationFeatureState& feature_state);
  static bool IsIncognitoAllowedInPolicy(int policy_value);

  static bool IsGuestAllowedInFeature(
      const AmbientAuthenticationFeatureState& feature_state);
  static bool IsGuestAllowedInPolicy(int policy_value);

  static Profile* GetGuestProfile();
  // OpenGuestBrowser method code borrowed from
  // chrome/browser/profiles/profile_window_browsertest.cc
  static Browser* OpenGuestBrowser();

  static void CookTheFeatureList(
      base::test::ScopedFeatureList& scoped_feature_list,
      const AmbientAuthenticationFeatureState& feature_state);
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_
