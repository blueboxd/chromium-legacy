// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_top_host_provider_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/previews/content/previews_hints_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsHostBlacklisted(const base::DictionaryValue* top_host_blacklist,
                       const std::string& host) {
  if (!top_host_blacklist)
    return false;
  return top_host_blacklist->FindKey(previews::HashHostForDictionary(host));
}

}  // namespace

namespace previews {
using namespace optimization_guide::prefs;

PreviewsTopHostProviderImpl::PreviewsTopHostProviderImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      pref_service_(Profile::FromBrowserContext(browser_context_)->GetPrefs()) {
}

PreviewsTopHostProviderImpl::~PreviewsTopHostProviderImpl() {}

void PreviewsTopHostProviderImpl::InitializeHintsFetcherTopHostBlacklist() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK_EQ(GetCurrentBlacklistState(),
            HintsFetcherTopHostBlacklistState::kNotInitialized);

  // TODO(mcrouse): When the blacklist state is kNotInitialized, the
  // |kHintsFetcherTopHostBlacklist| dictionary should be populated with the
  // hosts currently in the Site Engagement Service.
  UpdateCurrentBlacklistState(HintsFetcherTopHostBlacklistState::kInitialized);
}

// static
void PreviewsTopHostProviderImpl::MaybeUpdateTopHostBlacklist(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  PrefService* pref_service =
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs();

  if (pref_service->GetInteger(kHintsFetcherTopHostBlacklistState) !=
      static_cast<int>(HintsFetcherTopHostBlacklistState::kInitialized)) {
    return;
  }

  DictionaryPrefUpdate blacklist_pref(pref_service,
                                      kHintsFetcherTopHostBlacklist);
  if (!blacklist_pref->FindKey(previews::HashHostForDictionary(
          navigation_handle->GetURL().host()))) {
    return;
  }
  blacklist_pref->RemovePath(
      previews::HashHostForDictionary(navigation_handle->GetURL().host()));
  if (blacklist_pref->empty()) {
    blacklist_pref->Clear();
    pref_service->SetInteger(
        kHintsFetcherTopHostBlacklistState,
        static_cast<int>(HintsFetcherTopHostBlacklistState::kEmpty));
  }
}

HintsFetcherTopHostBlacklistState
PreviewsTopHostProviderImpl::GetCurrentBlacklistState() const {
  return static_cast<HintsFetcherTopHostBlacklistState>(
      pref_service_->GetInteger(kHintsFetcherTopHostBlacklistState));
}

void PreviewsTopHostProviderImpl::UpdateCurrentBlacklistState(
    HintsFetcherTopHostBlacklistState new_state) {
  HintsFetcherTopHostBlacklistState current_state = GetCurrentBlacklistState();
  // TODO(mcrouse): Change to DCHECK_NE.
  DCHECK_EQ(
      new_state == HintsFetcherTopHostBlacklistState::kInitialized,
      current_state == HintsFetcherTopHostBlacklistState::kNotInitialized &&
          new_state == HintsFetcherTopHostBlacklistState::kInitialized);

  DCHECK_EQ(new_state == HintsFetcherTopHostBlacklistState::kEmpty,
            current_state == HintsFetcherTopHostBlacklistState::kInitialized &&
                new_state == HintsFetcherTopHostBlacklistState::kEmpty);

  DCHECK_EQ(
      new_state == HintsFetcherTopHostBlacklistState::kNotInitialized,
      current_state == HintsFetcherTopHostBlacklistState::kEmpty &&
          new_state == HintsFetcherTopHostBlacklistState::kNotInitialized);

  if (current_state == new_state)
    return;

  // TODO(mcrouse): Add histogram to record the blacklist state change.
  pref_service_->SetInteger(kHintsFetcherTopHostBlacklistState,
                            static_cast<int>(new_state));
}

std::vector<std::string> PreviewsTopHostProviderImpl::GetTopHosts(
    size_t max_sites) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK(pref_service_);

  if (GetCurrentBlacklistState() ==
      HintsFetcherTopHostBlacklistState::kNotInitialized) {
    InitializeHintsFetcherTopHostBlacklist();
    return std::vector<std::string>();
  }

  // Create SiteEngagementService to request site engagement scores.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  const base::DictionaryValue* top_host_blacklist = nullptr;
  if (GetCurrentBlacklistState() != HintsFetcherTopHostBlacklistState::kEmpty) {
    top_host_blacklist =
        pref_service_->GetDictionary(kHintsFetcherTopHostBlacklist);
    // This check likely should not be needed as the process of removing hosts
    // from the blacklist should check and update the pref state.
    if (top_host_blacklist->size() == 0) {
      UpdateCurrentBlacklistState(HintsFetcherTopHostBlacklistState::kEmpty);
      top_host_blacklist = nullptr;
    }
    // TODO(mcrouse): Add histogram to record the size of the blacklist on
    // request for top hosts.
  }

  std::vector<std::string> top_hosts;
  top_hosts.reserve(max_sites);

  // Create a vector of the top hosts by engagement score up to |max_sites|
  // size. Currently utilizes just the first |max_sites| entries. Only HTTPS
  // schemed hosts are included. Hosts are filtered by the blacklist that is
  // populated when DataSaver is first enabled.
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();
  for (const auto& detail : engagement_details) {
    if (top_hosts.size() >= max_sites)
      return top_hosts;
    // TODO(b/968542): Skip origins that are local hosts (e.g., IP addresses,
    // localhost:8080 etc.).
    if (detail.origin.SchemeIs(url::kHttpsScheme) &&
        !IsHostBlacklisted(top_host_blacklist, detail.origin.host())) {
      top_hosts.push_back(detail.origin.host());
    }
  }

  return top_hosts;
}

}  // namespace previews
