// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_delegate_mac.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/mac/app_shim.mojom.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UpdateAppBadgeResult : int {
  kSuccess = 0,
  kNoShimManager = 1,
  kNoShimHost = 2,
  kNoAppShim = 3,
  kMaxValue = kNoAppShim
};

constexpr const char* kBadgeMetricName = "Badging.AppBadgeUpdate.Mac.Result";

// Records UMA metric for updating app badge.
void RecordUpdateAppBadge(const UpdateAppBadgeResult result) {
  UMA_HISTOGRAM_ENUMERATION(kBadgeMetricName, result);
}

}  // namespace

namespace badging {

BadgeManagerDelegateMac::BadgeManagerDelegateMac(Profile* profile,
                                                 BadgeManager* badge_manager)
    : BadgeManagerDelegate(profile, badge_manager) {}

void BadgeManagerDelegateMac::OnAppBadgeUpdated(const web_app::AppId& app_id) {
  const base::Optional<BadgeManager::BadgeValue>& badge =
      badge_manager()->GetBadgeValue(app_id);
  SetAppBadgeLabel(app_id, badge ? badging::GetBadgeString(badge.value()) : "");
}

void BadgeManagerDelegateMac::SetAppBadgeLabel(const std::string& app_id,
                                               const std::string& badge_label) {
  auto* shim_manager = apps::AppShimManager::Get();
  if (!shim_manager) {
    RecordUpdateAppBadge(UpdateAppBadgeResult::kNoShimManager);
    return;
  }

  // On macOS all app instances share a dock icon, so we only need to set the
  // badge label once.
  AppShimHost* shim_host = shim_manager->FindHost(profile(), app_id);
  if (!shim_host) {
    RecordUpdateAppBadge(UpdateAppBadgeResult::kNoShimHost);
    return;
  }
  chrome::mojom::AppShim* shim = shim_host->GetAppShim();
  if (!shim) {
    RecordUpdateAppBadge(UpdateAppBadgeResult::kNoAppShim);
    return;
  }
  shim->SetBadgeLabel(badge_label);
  RecordUpdateAppBadge(UpdateAppBadgeResult::kSuccess);
}

}  // namespace badging
