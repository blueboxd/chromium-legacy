// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/reputation_web_contents_observer.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_entry.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

void RecordHeuristicsUKMData(ReputationCheckResult result,
                             ukm::SourceId navigation_source_id,
                             SafetyTipInteraction action) {
  // If we didn't trigger any heuristics at all, we don't want to record UKM
  // data.
  if (!result.triggered_heuristics.triggered_any()) {
    return;
  }

  ukm::builders::Security_SafetyTip(navigation_source_id)
      .SetSafetyTipStatus(static_cast<int64_t>(result.safety_tip_status))
      .SetSafetyTipInteraction(static_cast<int64_t>(action))
      .SetTriggeredKeywordsHeuristics(
          result.triggered_heuristics.keywords_heuristic_triggered)
      .SetTriggeredLookalikeHeuristics(
          result.triggered_heuristics.lookalike_heuristic_triggered)
      .SetTriggeredServerSideBlocklist(
          result.triggered_heuristics.blocklist_heuristic_triggered)
      .SetUserPreviouslyIgnored(
          result.safety_tip_status ==
              security_state::SafetyTipStatus::kBadReputationIgnored ||
          result.safety_tip_status ==
              security_state::SafetyTipStatus::kLookalikeIgnored)
      .Record(ukm::UkmRecorder::Get());
}

void OnSafetyTipClosed(ReputationCheckResult result,
                       base::Time start_time,
                       ukm::SourceId navigation_source_id,
                       SafetyTipInteraction action) {
  std::string action_suffix;
  bool warning_dismissed = false;
  switch (action) {
    case SafetyTipInteraction::kNoAction:
      action_suffix = "NoAction";
      break;
    case SafetyTipInteraction::kLeaveSite:
      action_suffix = "LeaveSite";
      break;
    case SafetyTipInteraction::kDismiss:
      NOTREACHED();
      // Do nothing because the dismissal action passed to this method should
      // be the more specific version (esc, close, or ignore).
      break;
    case SafetyTipInteraction::kDismissWithEsc:
      action_suffix = "DismissWithEsc";
      warning_dismissed = true;
      break;
    case SafetyTipInteraction::kDismissWithClose:
      action_suffix = "DismissWithClose";
      warning_dismissed = true;
      break;
    case SafetyTipInteraction::kDismissWithIgnore:
      action_suffix = "DismissWithIgnore";
      warning_dismissed = true;
      break;
    case SafetyTipInteraction::kLearnMore:
      action_suffix = "LearnMore";
      break;
    case SafetyTipInteraction::kNotShown:
      NOTREACHED();
      // Do nothing because the OnSafetyTipClosed should never be called if the
      // safety tip is not shown.
      break;
  }
  if (warning_dismissed) {
    base::UmaHistogramCustomTimes(
        security_state::GetSafetyTipHistogramName(
            std::string("Security.SafetyTips.OpenTime.Dismiss"),
            result.safety_tip_status),
        base::Time::Now() - start_time, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromHours(1), 100);
  }
  base::UmaHistogramCustomTimes(
      security_state::GetSafetyTipHistogramName(
          std::string("Security.SafetyTips.OpenTime.") + action_suffix,
          result.safety_tip_status),
      base::Time::Now() - start_time, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromHours(1), 100);

  RecordHeuristicsUKMData(result, navigation_source_id, action);
}

}  // namespace

ReputationWebContentsObserver::~ReputationWebContentsObserver() = default;

void ReputationWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    MaybeCallReputationCheckCallback();
    return;
  }

  last_navigation_safety_tip_info_ = {security_state::SafetyTipStatus::kUnknown,
                                      GURL()};
  last_safety_tip_navigation_entry_id_ = 0;

  MaybeShowSafetyTip(
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID),
      /*record_ukm_if_tip_not_shown=*/true);
}

void ReputationWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  MaybeShowSafetyTip(ukm::GetSourceIdForWebContentsDocument(web_contents()),
                     /*record_ukm_if_tip_not_shown=*/false);
}

security_state::SafetyTipInfo
ReputationWebContentsObserver::GetSafetyTipInfoForVisibleNavigation() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return {security_state::SafetyTipStatus::kUnknown, GURL()};
  return last_safety_tip_navigation_entry_id_ == entry->GetUniqueID()
             ? last_navigation_safety_tip_info_
             : security_state::SafetyTipInfo(
                   {security_state::SafetyTipStatus::kUnknown, GURL()});
}

void ReputationWebContentsObserver::RegisterReputationCheckCallbackForTesting(
    base::OnceClosure callback) {
  reputation_check_callback_for_testing_ = std::move(callback);
}

ReputationWebContentsObserver::ReputationWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      weak_factory_(this) {
  last_navigation_safety_tip_info_ = {security_state::SafetyTipStatus::kUnknown,
                                      GURL()};
}

void ReputationWebContentsObserver::MaybeShowSafetyTip(
    ukm::SourceId navigation_source_id,
    bool record_ukm_if_tip_not_shown) {
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
      url, base::BindOnce(
               &ReputationWebContentsObserver::HandleReputationCheckResult,
               weak_factory_.GetWeakPtr(), navigation_source_id,
               record_ukm_if_tip_not_shown));
}

void ReputationWebContentsObserver::HandleReputationCheckResult(
    ukm::SourceId navigation_source_id,
    bool record_ukm_if_tip_not_shown,
    ReputationCheckResult result) {
  UMA_HISTOGRAM_ENUMERATION("Security.SafetyTips.SafetyTipShown",
                            result.safety_tip_status);

  // Set this field independent of whether the feature to show the UI is
  // enabled/disabled. Metrics code uses this field and we want to record
  // metrics regardless of the feature being enabled/disabled.
  last_navigation_safety_tip_info_ = {result.safety_tip_status,
                                      result.suggested_url};

  // A navigation entry should always exist because reputation checks are only
  // triggered when a committed navigation finishes.
  last_safety_tip_navigation_entry_id_ =
      web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID();
  // Since we downgrade indicator when a safety tip is triggered, update the
  // visible security state if we have a non-kNone status. This has to happen
  // after last_safety_tip_navigation_entry_id_ is updated.
  if (result.safety_tip_status != security_state::SafetyTipStatus::kNone) {
    web_contents()->DidChangeVisibleSecurityState();
  }

  if (result.safety_tip_status == security_state::SafetyTipStatus::kNone ||
      result.safety_tip_status ==
          security_state::SafetyTipStatus::kBadKeyword) {
    FinalizeReputationCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                           navigation_source_id);
    return;
  }

  if (result.safety_tip_status ==
          security_state::SafetyTipStatus::kLookalikeIgnored ||
      result.safety_tip_status ==
          security_state::SafetyTipStatus::kBadReputationIgnored) {
    UMA_HISTOGRAM_ENUMERATION("Security.SafetyTips.SafetyTipIgnoredPageLoad",
                              result.safety_tip_status);
    FinalizeReputationCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                           navigation_source_id);
    return;
  }

  if (!base::FeatureList::IsEnabled(security_state::features::kSafetyTipUI)) {
    // When the feature isn't enabled, we 'ignore' the UI after the first visit
    // to make it easier to disambiguate the control groups' first visit from
    // subsequent navigations to the flagged page in metrics. Since the user
    // never sees the UI, this is a no-op from their perspective.
    if (result.safety_tip_status ==
            security_state::SafetyTipStatus::kLookalike ||
        result.safety_tip_status ==
            security_state::SafetyTipStatus::kBadReputation) {
      ReputationService::Get(profile_)->OnUIDisabledFirstVisit(result.url);
    }

    FinalizeReputationCheckWhenTipNotShown(record_ukm_if_tip_not_shown, result,
                                           navigation_source_id);
    return;
  }

  ShowSafetyTipDialog(web_contents(), result.safety_tip_status, result.url,
                      result.suggested_url,
                      base::BindOnce(OnSafetyTipClosed, result,
                                     base::Time::Now(), navigation_source_id));
  MaybeCallReputationCheckCallback();
}

void ReputationWebContentsObserver::MaybeCallReputationCheckCallback() {
  if (reputation_check_callback_for_testing_.is_null())
    return;
  std::move(reputation_check_callback_for_testing_).Run();
}

void ReputationWebContentsObserver::FinalizeReputationCheckWhenTipNotShown(
    bool record_ukm,
    ReputationCheckResult result,
    ukm::SourceId navigation_source_id) {
  if (record_ukm) {
    RecordHeuristicsUKMData(result, navigation_source_id,
                            SafetyTipInteraction::kNotShown);
  }
  MaybeCallReputationCheckCallback();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReputationWebContentsObserver)
