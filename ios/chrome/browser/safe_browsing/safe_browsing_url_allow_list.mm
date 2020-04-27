// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_url_allow_list.h"

#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using safe_browsing::SBThreatType;

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingUrlAllowList)

SafeBrowsingUrlAllowList::SafeBrowsingUrlAllowList(web::WebState* web_state)
    : web_state_(web_state) {}

SafeBrowsingUrlAllowList::SafeBrowsingUrlAllowList(
    SafeBrowsingUrlAllowList&& other) = default;

SafeBrowsingUrlAllowList& SafeBrowsingUrlAllowList::operator=(
    SafeBrowsingUrlAllowList&& other) = default;

SafeBrowsingUrlAllowList::~SafeBrowsingUrlAllowList() = default;

bool SafeBrowsingUrlAllowList::AreUnsafeNavigationsAllowed(
    const GURL& url,
    std::set<SBThreatType>* threat_types) {
  auto& allowed_threats = GetUnsafeNavigationDecisions(url).allowed_threats;
  if (allowed_threats.empty())
    return false;
  if (threat_types)
    *threat_types = allowed_threats;
  return true;
}

void SafeBrowsingUrlAllowList::AllowUnsafeNavigations(
    const GURL& url,
    SBThreatType threat_type) {
  UnsafeNavigationDecisions& whitelist_decisions =
      GetUnsafeNavigationDecisions(url);
  whitelist_decisions.allowed_threats.insert(threat_type);
  whitelist_decisions.pending_threats.erase(threat_type);
  web_state_->DidChangeVisibleSecurityState();
}

void SafeBrowsingUrlAllowList::DisallowUnsafeNavigations(const GURL& url) {
  GetUnsafeNavigationDecisions(url).allowed_threats.clear();
  web_state_->DidChangeVisibleSecurityState();
}

bool SafeBrowsingUrlAllowList::IsUnsafeNavigationDecisionPending(
    const GURL& url,
    std::set<SBThreatType>* threat_types) {
  auto& pending_threats = GetUnsafeNavigationDecisions(url).pending_threats;
  if (pending_threats.empty())
    return false;
  if (threat_types)
    *threat_types = pending_threats;
  return true;
}

void SafeBrowsingUrlAllowList::AddPendingUnsafeNavigationDecision(
    const GURL& url,
    SBThreatType threat_type) {
  GetUnsafeNavigationDecisions(url).pending_threats.insert(threat_type);
  web_state_->DidChangeVisibleSecurityState();
}

void SafeBrowsingUrlAllowList::RemovePendingUnsafeNavigationDecisions(
    const GURL& url) {
  GetUnsafeNavigationDecisions(url).pending_threats.clear();
  web_state_->DidChangeVisibleSecurityState();
}

SafeBrowsingUrlAllowList::UnsafeNavigationDecisions&
SafeBrowsingUrlAllowList::GetUnsafeNavigationDecisions(const GURL& url) {
  return decisions_[url.GetWithEmptyPath()];
}

SafeBrowsingUrlAllowList::UnsafeNavigationDecisions::
    UnsafeNavigationDecisions() = default;

SafeBrowsingUrlAllowList::UnsafeNavigationDecisions::
    ~UnsafeNavigationDecisions() = default;
