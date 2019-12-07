// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "components/safe_browsing/db/database_manager.h"

namespace {

using UiToUse = ContextualNotificationPermissionUiSelector::UiToUse;
using QuietUiReason = ContextualNotificationPermissionUiSelector::QuietUiReason;

// Attempts to decide which UI to use based on preloaded site reputation data,
// or returns base::nullopt if not possible. |site_reputation| can be nullptr.
base::Optional<UiToUse> GetUiToUseBasedOnSiteReputation(
    const CrowdDenyPreloadData::SiteReputation* site_reputation) {
  if (!site_reputation)
    return base::nullopt;

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

}  // namespace

ContextualNotificationPermissionUiSelector::
    ContextualNotificationPermissionUiSelector(Profile* profile)
    : profile_(profile) {}

void ContextualNotificationPermissionUiSelector::SelectUiToUse(
    PermissionRequest* request,
    DecisionMadeCallback callback) {
  callback_ = std::move(callback);
  DCHECK(callback_);

  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts)) {
    Notify(UiToUse::kNormalUi, base::nullopt);
    return;
  }

  if (QuietNotificationPermissionUiState::IsQuietUiEnabledInPrefs(profile_)) {
    Notify(UiToUse::kQuietUi, QuietUiReason::kEnabledInPrefs);
    return;
  }

  if (!QuietNotificationPermissionUiConfig::IsCrowdDenyTriggeringEnabled()) {
    Notify(UiToUse::kNormalUi, base::nullopt);
    return;
  }

  const auto origin = url::Origin::Create(request->GetOrigin());
  base::Optional<UiToUse> ui_to_use = GetUiToUseBasedOnSiteReputation(
      CrowdDenyPreloadData::GetInstance()->GetReputationDataForSite(origin));
  if (ui_to_use) {
    Notify(*ui_to_use, QuietUiReason::kTriggeredByCrowdDeny);
    return;
  }

  DCHECK(!safe_browsing_request_);
  DCHECK(g_browser_process->safe_browsing_service());

  // It is fine to use base::Unretained() here, as |safe_browsing_request_|
  // guarantees not to fire the callback after its destruction.
  safe_browsing_request_.emplace(
      g_browser_process->safe_browsing_service()->database_manager(), origin,
      base::BindOnce(&ContextualNotificationPermissionUiSelector::
                         OnSafeBrowsingVerdictReceived,
                     base::Unretained(this)));
}

void ContextualNotificationPermissionUiSelector::Cancel() {
  // The computation either finishes synchronously above, or is waiting on the
  // Safe Browsing check.
  safe_browsing_request_.reset();
}

ContextualNotificationPermissionUiSelector::
    ~ContextualNotificationPermissionUiSelector() = default;

void ContextualNotificationPermissionUiSelector::OnSafeBrowsingVerdictReceived(
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  DCHECK(safe_browsing_request_);
  DCHECK(callback_);
  safe_browsing_request_.reset();
  Notify(GetUiToUseFromSafeBrowsingVerdict(verdict),
         QuietUiReason::kTriggeredByCrowdDeny);
}

void ContextualNotificationPermissionUiSelector::Notify(
    UiToUse ui_to_use,
    base::Optional<QuietUiReason> quiet_ui_reason) {
  if (ui_to_use != UiToUse::kQuietUi)
    quiet_ui_reason = base::nullopt;
  std::move(callback_).Run(ui_to_use, quiet_ui_reason);
}
