// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_top_host_provider.h"

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsHostBlacklisted(const base::DictionaryValue* top_host_blacklist,
                       const std::string& host) {
  if (!top_host_blacklist)
    return false;
  return top_host_blacklist->FindKey(
      optimization_guide::HashHostForDictionary(host));
}

}  // namespace

PreviewsTopHostProvider::PreviewsTopHostProvider(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      pref_service_(Profile::FromBrowserContext(browser_context_)->GetPrefs()) {
}

PreviewsTopHostProvider::~PreviewsTopHostProvider() {}

void PreviewsTopHostProvider::InitializeHintsFetcherTopHostBlacklist() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK_EQ(GetCurrentBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kNotInitialized);
  DCHECK(pref_service_
             ->GetDictionary(
                 optimization_guide::prefs::kHintsFetcherTopHostBlacklist)
             ->empty());

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  std::unique_ptr<base::DictionaryValue> top_host_blacklist =
      std::make_unique<base::DictionaryValue>();

  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();

  std::sort(engagement_details.begin(), engagement_details.end(),
            [](const mojom::SiteEngagementDetails& lhs,
               const mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score > rhs.total_score;
            });

  for (const auto& detail : engagement_details) {
    if (top_host_blacklist->size() >=
        optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize()) {
      break;
    }
    if (detail.origin.SchemeIsHTTPOrHTTPS()) {
      top_host_blacklist->SetBoolKey(
          optimization_guide::HashHostForDictionary(detail.origin.host()),
          true);
    }
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "OptimizationGuide.HintsFetcher.TopHostProvider.BlacklistSize."
      "OnInitialize",
      top_host_blacklist->size());

  pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlacklist,
                     *top_host_blacklist);

  UpdateCurrentBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
          kInitialized);
}

// static
void PreviewsTopHostProvider::MaybeUpdateTopHostBlacklist(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  PrefService* pref_service =
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs();

  if (pref_service->GetInteger(
          optimization_guide::prefs::kHintsFetcherTopHostBlacklistState) !=
      static_cast<int>(optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kInitialized)) {
    return;
  }

  DictionaryPrefUpdate blacklist_pref(
      pref_service, optimization_guide::prefs::kHintsFetcherTopHostBlacklist);
  if (!blacklist_pref->FindKey(optimization_guide::HashHostForDictionary(
          navigation_handle->GetURL().host()))) {
    return;
  }
  blacklist_pref->RemovePath(optimization_guide::HashHostForDictionary(
      navigation_handle->GetURL().host()));
  if (blacklist_pref->empty()) {
    blacklist_pref->Clear();
    pref_service->SetInteger(
        optimization_guide::prefs::kHintsFetcherTopHostBlacklistState,
        static_cast<int>(optimization_guide::prefs::
                             HintsFetcherTopHostBlacklistState::kEmpty));
  }
}

optimization_guide::prefs::HintsFetcherTopHostBlacklistState
PreviewsTopHostProvider::GetCurrentBlacklistState() const {
  return static_cast<
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState>(
      pref_service_->GetInteger(
          optimization_guide::prefs::kHintsFetcherTopHostBlacklistState));
}

void PreviewsTopHostProvider::UpdateCurrentBlacklistState(
    optimization_guide::prefs::HintsFetcherTopHostBlacklistState new_state) {
  optimization_guide::prefs::HintsFetcherTopHostBlacklistState current_state =
      GetCurrentBlacklistState();
  // TODO(mcrouse): Change to DCHECK_NE.
  DCHECK_EQ(
      new_state == optimization_guide::prefs::
                       HintsFetcherTopHostBlacklistState::kInitialized,
      current_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kNotInitialized &&
          new_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kInitialized);

  DCHECK_EQ(
      new_state ==
          optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty,
      current_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kInitialized &&
          new_state == optimization_guide::prefs::
                           HintsFetcherTopHostBlacklistState::kEmpty);

  DCHECK_EQ(new_state == optimization_guide::prefs::
                             HintsFetcherTopHostBlacklistState::kNotInitialized,
            current_state == optimization_guide::prefs::
                                 HintsFetcherTopHostBlacklistState::kEmpty &&
                new_state ==
                    optimization_guide::prefs::
                        HintsFetcherTopHostBlacklistState::kNotInitialized);

  if (current_state == new_state)
    return;

  // TODO(mcrouse): Add histogram to record the blacklist state change.
  pref_service_->SetInteger(
      optimization_guide::prefs::kHintsFetcherTopHostBlacklistState,
      static_cast<int>(new_state));
}

std::vector<std::string> PreviewsTopHostProvider::GetTopHosts(
    size_t max_sites) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK(pref_service_);

  if (GetCurrentBlacklistState() ==
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
          kNotInitialized) {
    InitializeHintsFetcherTopHostBlacklist();
    return std::vector<std::string>();
  }

  // Create SiteEngagementService to request site engagement scores.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  const base::DictionaryValue* top_host_blacklist = nullptr;
  if (GetCurrentBlacklistState() !=
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty) {
    top_host_blacklist = pref_service_->GetDictionary(
        optimization_guide::prefs::kHintsFetcherTopHostBlacklist);
    UMA_HISTOGRAM_COUNTS_1000(
        "OptimizationGuide.HintsFetcher.TopHostProvider.BlacklistSize."
        "OnRequest",
        top_host_blacklist->size());
    // This check likely should not be needed as the process of removing hosts
    // from the blacklist should check and update the pref state.
    if (top_host_blacklist->size() == 0) {
      UpdateCurrentBlacklistState(
          optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty);
      top_host_blacklist = nullptr;
    }
  }

  std::vector<std::string> top_hosts;
  top_hosts.reserve(max_sites);

  // Create a vector of the top hosts by engagement score up to |max_sites|
  // size. Currently utilizes just the first |max_sites| entries. Only HTTPS
  // schemed hosts are included. Hosts are filtered by the blacklist that is
  // populated when DataSaver is first enabled.
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();

  std::sort(engagement_details.begin(), engagement_details.end(),
            [](const mojom::SiteEngagementDetails& lhs,
               const mojom::SiteEngagementDetails& rhs) {
              return lhs.total_score > rhs.total_score;
            });

  for (const auto& detail : engagement_details) {
    if (top_hosts.size() >= max_sites)
      return top_hosts;
    // Once the engagement score is less than the initial engagement score for a
    // newly navigated host, return the current set of top hosts. This threshold
    // prevents hosts that have not been engaged recently from having hints
    // requested for them. The engagement_details are sorted above in descending
    // order by engagement score.
    if (detail.total_score <= GetMinTopHostEngagementThreshold())
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

size_t PreviewsTopHostProvider::GetMinTopHostEngagementThreshold() const {
  // The base score for the first navigation of a host when added to the site
  // engagement service. The threshold corresponds to the minimum score that a
  // host is considered to be a top host, hosts with a lower score have not
  // been navigated to recently.
  return SiteEngagementScore::GetNavigationPoints() +
         SiteEngagementScore::GetFirstDailyEngagementPoints();
}
