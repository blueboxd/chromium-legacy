// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/reputation_web_contents_observer.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_entry.h"

namespace safety_tips {

ReputationWebContentsObserver::~ReputationWebContentsObserver() {}

void ReputationWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  last_navigation_safety_tip_type_ = SafetyTipType::kNone;
  last_safety_tip_navigation_entry_id_ = 0;

  MaybeShowSafetyTip();
}

void ReputationWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  MaybeShowSafetyTip();
}

SafetyTipType
ReputationWebContentsObserver::GetSafetyTipTypeForVisibleNavigation() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return SafetyTipType::kNone;
  return last_safety_tip_navigation_entry_id_ == entry->GetUniqueID()
             ? last_navigation_safety_tip_type_
             : SafetyTipType::kNone;
}

ReputationWebContentsObserver::ReputationWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      weak_factory_(this) {}

void ReputationWebContentsObserver::MaybeShowSafetyTip() {
  if (web_contents()->GetMainFrame()->GetVisibilityState() !=
      content::PageVisibilityState::kVisible) {
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

void ReputationWebContentsObserver::HandleReputationCheckResult(
    SafetyTipType type,
    bool user_ignored,
    const GURL& url) {
  if (type == SafetyTipType::kNone) {
    return;
  }

  // TODO(crbug/987754): Record metrics here.

  if (user_ignored) {
    return;
  }
  // Set this field independent of whether the feature to show the UI is
  // enabled/disabled. Metrics code uses this field and we want to record
  // metrics regardless of the feature being enabled/disabled.
  last_navigation_safety_tip_type_ = type;
  // A navigation entry should always exist because reputation checks are only
  // triggered when a committed navigation finishes.
  last_safety_tip_navigation_entry_id_ =
      web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();

  if (!base::FeatureList::IsEnabled(features::kSafetyTipUI)) {
    return;
  }
  ShowSafetyTipDialog(web_contents(), type, url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReputationWebContentsObserver)

}  // namespace safety_tips
