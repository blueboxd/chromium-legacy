// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/reputation_web_contents_observer.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

namespace safety_tips {

ReputationWebContentsObserver::~ReputationWebContentsObserver() {}

void ReputationWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  ReputationService* service = ReputationService::Get(profile_);
  service->GetReputationStatus(
      url, base::BindRepeating(
               &ReputationWebContentsObserver::HandleReputationCheckResult,
               weak_factory_.GetWeakPtr()));
}

ReputationWebContentsObserver::ReputationWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      weak_factory_(this) {}

void ReputationWebContentsObserver::HandleReputationCheckResult(
    SafetyTipType type,
    bool user_ignored,
    const GURL& url) {
#if !defined(OS_ANDROID)
  if (type == SafetyTipType::kNone) {
    return;
  }

  // TODO(crbug/987754): Record metrics here, and make sure OS_ANDROID checking
  // happens after this point when metrics land.

  if (user_ignored || !base::FeatureList::IsEnabled(features::kSafetyTipUI)) {
    return;
  }

  ShowSafetyTipDialog(web_contents(), type, url);
#endif  // !defined(OS_ANDROID)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReputationWebContentsObserver)

}  // namespace safety_tips
