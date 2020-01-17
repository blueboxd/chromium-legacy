// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace features {

const base::Feature kIgnoresViewportScaleLimits{
    "IgnoresViewportScaleLimits", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCrashOnUnexpectedURLChange{
    "CrashOnUnexpectedURLChange", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHistoryClobberWorkaround{
    "WKWebViewHistoryClobberWorkaround", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBlockUniversalLinksInOffTheRecordMode{
    "BlockUniversalLinksInOffTheRecord", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kKeepsRenderProcessAlive{"KeepsRenderProcessAlive",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClearOldNavigationRecordsWorkaround{
    "ClearOldNavigationRecordsWorkaround", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSSLCommittedInterstitials{
    "SSLCommittedInterstitials", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseWKWebViewLoading{"UseWKWebViewLoading",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnablePersistentDownloads{
    "EnablePersistentDownloads", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseJSForErrorPage{"UseJSForErrorPage",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseDefaultUserAgentInWebClient{
    "UseDefaultUserAgentInWebClient", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreserveScrollViewProperties{
    "PreserveScrollViewProperties", base::FEATURE_DISABLED_BY_DEFAULT};

bool UseWKWebViewLoading() {
  return base::FeatureList::IsEnabled(web::features::kUseWKWebViewLoading);
}

}  // namespace features
}  // namespace web
