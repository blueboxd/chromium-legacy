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
using UiToUseWithReason = std::pair<UiToUse, base::Optional<QuietUiReason>>;

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

// Records a histogram sample for the |warning_only| bit.
void RecordWarningOnlyState(bool value) {
  base::UmaHistogramBoolean("Permissions.CrowdDeny.PreloadData.WarningOnly",
                            value);
}

// Attempts to decide which UI to use based on preloaded site reputation data,
// or returns base::nullopt if not possible. |site_reputation| can be nullptr.
base::Optional<UiToUseWithReason> GetUiToUseBasedOnSiteReputation(
    const CrowdDenyPreloadData::SiteReputation* site_reputation) {
  if (!site_reputation) {
    RecordNotificationUserExperienceQuality(
        CrowdDenyPreloadData::SiteReputation::UNKNOWN);
    return base::nullopt;
  }

  RecordNotificationUserExperienceQuality(
      site_reputation->notification_ux_quality());
  RecordWarningOnlyState(site_reputation->warning_only());

  switch (site_reputation->notification_ux_quality()) {
    case CrowdDenyPreloadData::SiteReputation::ACCEPTABLE:
      return std::make_pair(UiToUse::kNormalUi, base::nullopt);
    case CrowdDenyPreloadData::SiteReputation::UNSOLICITED_PROMPTS:
      if (!QuietNotificationPermissionUiConfig::IsCrowdDenyTriggeringEnabled())
        return base::nullopt;
      if (site_reputation->warning_only())
        return std::make_pair(UiToUse::kNormalUi, base::nullopt);
      return std::make_pair(UiToUse::kQuietUi,
                            QuietUiReason::kTriggeredByCrowdDeny);
    case CrowdDenyPreloadData::SiteReputation::ABUSIVE_PROMPTS:
      if (!QuietNotificationPermissionUiConfig::
              IsAbusiveRequestBlockingEnabled()) {
        return base::nullopt;
      }
      if (site_reputation->warning_only())
        return std::make_pair(UiToUse::kNormalUi, base::nullopt);
      return std::make_pair(UiToUse::kQuietUi,
                            QuietUiReason::kTriggeredDueToAbusiveRequests);
    case CrowdDenyPreloadData::SiteReputation::UNKNOWN:
      return base::nullopt;
  }

  NOTREACHED();
  return base::nullopt;
}

// Roll the dice to decide whether to use the normal UI even when the preload
// data indicates that quiet UI should be used. This creates a control group of
// normal UI prompt impressions, which facilitates comparing acceptance rates,
// better calibrating server-side logic, and detecting when the notification
// experience on the site has improved.
bool ShouldHoldBackQuietUI(QuietUiReason quiet_ui_reason) {
  const double kHoldbackChance =
      QuietNotificationPermissionUiConfig::GetCrowdDenyHoldBackChance();

  // There is no hold-back when the quiet UI is shown due to abusive permission
  // request UX, as those verdicts are not calculated based on acceptance rates.
  if (quiet_ui_reason != QuietUiReason::kTriggeredByCrowdDeny)
    return false;

  // Avoid rolling a dice if the chance is 0.
  const bool result = kHoldbackChance && base::RandDouble() < kHoldbackChance;
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

  // Even if the quiet UI is enabled on all sites, the crowd deny and abuse
  // trigger conditions must be evaluated first, so that the corresponding,
  // less prominent UI and the strings are shown on blocklisted origins.
  EvaluatePerSiteTriggers(url::Origin::Create(request->GetOrigin()));
}

void ContextualNotificationPermissionUiSelector::Cancel() {
  // The computation either finishes synchronously above, or is waiting on the
  // Safe Browsing check.
  safe_browsing_request_.reset();
}

ContextualNotificationPermissionUiSelector::
    ~ContextualNotificationPermissionUiSelector() = default;

void ContextualNotificationPermissionUiSelector::EvaluatePerSiteTriggers(
    const url::Origin& origin) {
  auto ui_to_use_with_reason = GetUiToUseBasedOnSiteReputation(
      CrowdDenyPreloadData::GetInstance()->GetReputationDataForSite(origin));
  if (!ui_to_use_with_reason ||
      ui_to_use_with_reason->first == UiToUse::kNormalUi) {
    OnPerSiteTriggersEvaluated(UiToUse::kNormalUi, base::nullopt);
    return;
  }

  // PreloadData suggests an unacceptable site, ping Safe Browsing to verify.
  DCHECK(!safe_browsing_request_);
  DCHECK(g_browser_process->safe_browsing_service());

  // It is fine to use base::Unretained() here, as |safe_browsing_request_|
  // guarantees not to fire the callback after its destruction.
  safe_browsing_request_.emplace(
      g_browser_process->safe_browsing_service()->database_manager(),
      base::DefaultClock::GetInstance(), origin,
      base::BindOnce(&ContextualNotificationPermissionUiSelector::
                         OnSafeBrowsingVerdictReceived,
                     base::Unretained(this), *ui_to_use_with_reason->second));
}

void ContextualNotificationPermissionUiSelector::OnSafeBrowsingVerdictReceived(
    QuietUiReason candidate_quiet_ui_reason,
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  DCHECK(safe_browsing_request_);
  DCHECK(callback_);

  safe_browsing_request_.reset();

  switch (verdict) {
    case CrowdDenySafeBrowsingRequest::Verdict::kAcceptable:
      OnPerSiteTriggersEvaluated(UiToUse::kNormalUi, base::nullopt);
      break;
    case CrowdDenySafeBrowsingRequest::Verdict::kUnacceptable:
      OnPerSiteTriggersEvaluated(UiToUse::kQuietUi, candidate_quiet_ui_reason);
      break;
  }
}

void ContextualNotificationPermissionUiSelector::OnPerSiteTriggersEvaluated(
    UiToUse ui_to_use,
    base::Optional<QuietUiReason> quiet_ui_reason) {
  if (ui_to_use == UiToUse::kQuietUi &&
      !ShouldHoldBackQuietUI(*quiet_ui_reason)) {
    Notify(UiToUse::kQuietUi, quiet_ui_reason);
    return;
  }

  // Still show the quiet UI if it is enabled for all sites, even if per-site
  // conditions did not trigger showing the quiet UI on this origin.
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
