// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kChromeInterstitialSslPath[] = "/ssl";
const char kChromeInterstitialCaptivePortalPath[] = "/captiveportal";

const char kChromeInterstitialSslUrlQueryKey[] = "url";
const char kChromeInterstitialSslOverridableQueryKey[] = "overridable";
const char kChromeInterstitialSslStrictEnforcementQueryKey[] =
    "strict_enforcement";
const char kChromeInterstitialSslTypeQueryKey[] = "type";
const char kChromeInterstitialSslTypeHpkpFailureQueryValue[] = "hpkp_failure";
const char kChromeInterstitialSslTypeCtFailureQueryValue[] = "ct_failure";
