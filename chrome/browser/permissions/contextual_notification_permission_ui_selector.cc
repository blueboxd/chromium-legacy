// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/permissions/permission_request.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/db/database_manager.h"

namespace {

using UiToUse = ContextualNotificationPermissionUiSelector::UiToUse;
using QuietUiReason = ContextualNotificationPermissionUiSelector::QuietUiReason;

// Records a histogram sample for NotificationUserExperienceQuality.
void RecordNotificationUserExperienceQuality(
    CrowdDenyPreloadData::SiteReputation::NotificationUserExperienceQuality
        value) {
  // Cannot use either base::UmaHistogramEnumeration() overload here because
  // ARRAYSIZE is defined as MAX+1 but also as type `int`.
  base::UmaHistogramExactLinear(
      "Permissions.CrowdDeny.PreloadData.NotificationUxQuality",
      static_cast<int>(value),
      CrowdDenyPreloadData::SiteReputation::
          NotificationUserExperienceQuality_ARRAYSIZE);
}

// Attempts to decide which UI to use based on preloaded site reputation data,
// or returns base::nullopt if not possible. |site_reputation| can be nullptr.
base::Optional<UiToUse> GetUiToUseBasedOnSiteReputation(
    const CrowdDenyPreloadData::SiteReputation* site_reputation) {
  if (!site_reputation) {
    RecordNotificationUserExperienceQuality(
        CrowdDenyPreloadData::SiteReputation::UNKNOWN);
    return base::nullopt;
  }

  RecordNotificationUserExperienceQuality(
      site_reputation->notification_ux_quality());
  switch (site_reputation->notification_ux_quality()) {
    case CrowdDenyPreloadData::SiteReputation::ACCEPTABLE:
      return UiToUse::kNormalUi;
    case CrowdDenyPreloadData::SiteReputation::UNSOLICITED_PROMPTS:
      return UiToUse::kQuietUi;
    case CrowdDenyPreloadData::SiteReputation::UNKNOWN:
      return base::nullopt;
  }

  NOTREACHED();
  return base::nullopt;
}

// Decides which UI to use based on the Safe Browsing verdict.
UiToUse GetUiToUseFromSafeBrowsingVerdict(
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  using Verdict = CrowdDenySafeBrowsingRequest::Verdict;

  switch (verdict) {
    case Verdict::kAcceptable:
      return UiToUse::kNormalUi;
    case Verdict::kKnownToShowUnsolicitedNotificationPermissionRequests:
      return UiToUse::kQuietUi;
  }

  NOTREACHED();
  return UiToUse::kNormalUi;
}

// Roll the dice to decide whether to use the normal UI even when the crowd data
// suggests that a site is sending unsolicited prompts. This creates a control
// group of normal UI prompt impressions, which facilitates comparing acceptance
// rates, better calibrating server-side logic, and detecting when the
// notification experience on the site has improved.
bool ShouldHoldBackQuietUI() {
  const double chance =
      QuietNotificationPermissionUiConfig::GetCrowdDenyHoldBackChance();
  // Avoid rolling a dice if the chance is 0.
  const bool result = chance && base::RandDouble() < chance;
  base::UmaHistogramBoolean("Permissions.CrowdDeny.DidHoldbackQuietUi", result);
  return result;
}

}  // namespace

ContextualNotificationPermissionUiSelector::
    ContextualNotificationPermissionUiSelector(Profile* profile)
    : profile_(profile) {}

void ContextualNotificationPermissionUiSelector::SelectUiToUse(
    permissions::PermissionRequest* request,
    DecisionMadeCallback callback) {
  callback_ = std::move(callback);
  DCHECK(callback_);

  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts)) {
    Notify(UiToUse::kNormalUi, base::nullopt);
    return;
  }

  // Even if the quiet UI is enabled on all sites, the crowd deny trigger
  // condition must be evaluated, so that the less prominent UI and the correct
  // strings are shown on origins with crowd deny activated.
  EvaluateCrowdDenyTrigger(url::Origin::Create(request->GetOrigin()));
}

void ContextualNotificationPermissionUiSelector::Cancel() {
  // The computation either finishes synchronously above, or is waiting on the
  // Safe Browsing check.
  safe_browsing_request_.reset();
}

ContextualNotificationPermissionUiSelector::
    ~ContextualNotificationPermissionUiSelector() = default;

void ContextualNotificationPermissionUiSelector::EvaluateCrowdDenyTrigger(
    url::Origin origin) {
  if (!QuietNotificationPermissionUiConfig::IsCrowdDenyTriggeringEnabled()) {
    OnCrowdDenyTriggerEvaluated(UiToUse::kNormalUi);
    return;
  }

  base::Optional<UiToUse> ui_to_use = GetUiToUseBasedOnSiteReputation(
      CrowdDenyPreloadData::GetInstance()->GetReputationDataForSite(origin));
  if (!ui_to_use || *ui_to_use == UiToUse::kNormalUi) {
    OnCrowdDenyTriggerEvaluated(UiToUse::kNormalUi);
    return;
  }

  // PreloadData suggests a spammy site, ping safe browsing to verify.
  DCHECK(!safe_browsing_request_);
  DCHECK(g_browser_process->safe_browsing_service());

  // It is fine to use base::Unretained() here, as |safe_browsing_request_|
  // guarantees not to fire the callback after its destruction.
  safe_browsing_request_.emplace(
      g_browser_process->safe_browsing_service()->database_manager(),
      base::DefaultClock::GetInstance(), origin,
      base::BindOnce(&ContextualNotificationPermissionUiSelector::
                         OnSafeBrowsingVerdictReceived,
                     base::Unretained(this)));
}

void ContextualNotificationPermissionUiSelector::OnSafeBrowsingVerdictReceived(
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  DCHECK(safe_browsing_request_);
  DCHECK(callback_);
  safe_browsing_request_.reset();
  OnCrowdDenyTriggerEvaluated(GetUiToUseFromSafeBrowsingVerdict(verdict));
}

void ContextualNotificationPermissionUiSelector::OnCrowdDenyTriggerEvaluated(
    UiToUse ui_to_use) {
  if (ui_to_use == UiToUse::kQuietUi && !ShouldHoldBackQuietUI()) {
    Notify(UiToUse::kQuietUi, QuietUiReason::kTriggeredByCrowdDeny);
    return;
  }

  // Still show the quiet UI if it is enabled for all sites, even if crowd deny
  // did not trigger showing the quiet UI on this origin.
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kEnableQuietNotificationPermissionUi)) {
    Notify(UiToUse::kQuietUi, QuietUiReason::kEnabledInPrefs);
    return;
  }

  Notify(UiToUse::kNormalUi, base::nullopt);
}

void ContextualNotificationPermissionUiSelector::Notify(
    UiToUse ui_to_use,
    base::Optional<QuietUiReason> quiet_ui_reason) {
  DCHECK_EQ(ui_to_use == UiToUse::kQuietUi, !!quiet_ui_reason);
  std::move(callback_).Run(ui_to_use, quiet_ui_reason);
}

// static
