// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_FEATURES_H_
#define IOS_WEB_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Used to always allow scaling of the web page, regardless of author intent.
extern const base::Feature kIgnoresViewportScaleLimits;

// Used to enable the WKBackForwardList based navigation manager.
extern const base::Feature kSlimNavigationManager;

// Used to enable using WKHTTPSystemCookieStore in main context URL requests.
extern const base::Feature kWKHTTPSystemCookieStore;

// Used to crash the browser if unexpected URL change is detected.
// https://crbug.com/841105.
extern const base::Feature kCrashOnUnexpectedURLChange;

// Used to enable the workaround for WKWebView history clobber bug
// (crbug.com/887497).
extern const base::Feature kHistoryClobberWorkaround;

// Used to prevent native apps from being opened when a universal link is tapped
// and the user is browsing in off the record mode.
extern const base::Feature kBlockUniversalLinksInOffTheRecordMode;

// Used to ensure that the render is not suspended.
extern const base::Feature kKeepsRenderProcessAlive;

// Used to enable committed interstitials for SSL errors.
extern const base::Feature kSSLCommittedInterstitials;

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_COMMON_FEATURES_H_
