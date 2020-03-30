// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_CONSTANTS_H_

// Paths used by chrome://interstitials.
extern const char kChromeInterstitialSslPath[];
extern const char kChromeInterstitialCaptivePortalPath[];

// Query keys and values for chrome://interstitials/ssl
extern const char kChromeInterstitialSslUrlQueryKey[];
extern const char kChromeInterstitialSslOverridableQueryKey[];
extern const char kChromeInterstitialSslStrictEnforcementQueryKey[];
extern const char kChromeInterstitialSslTypeQueryKey[];
extern const char kChromeInterstitialSslTypeHpkpFailureQueryValue[];
extern const char kChromeInterstitialSslTypeCtFailureQueryValue[];

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_CONSTANTS_H_
