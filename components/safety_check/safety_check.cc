// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/safety_check.h"

#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safety_check {

SafetyCheck::SafetyCheck(SafetyCheckHandlerInterface* handler)
    : handler_(handler) {
  DCHECK(handler_);
}

SafetyCheck::~SafetyCheck() = default;

void SafetyCheck::CheckSafeBrowsing(PrefService* pref_service) {
  const PrefService::Preference* enabled_pref =
      pref_service->FindPreference(prefs::kSafeBrowsingEnabled);
  bool is_sb_enabled = pref_service->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool is_sb_managed = enabled_pref->IsManaged();

  SafeBrowsingStatus status;
  if (is_sb_enabled && pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced)) {
    status = SafeBrowsingStatus::kEnabledEnhanced;
  } else if (is_sb_enabled && is_sb_managed) {
    status = SafeBrowsingStatus::kEnabledStandard;
  } else if (is_sb_enabled && !is_sb_managed) {
    status = SafeBrowsingStatus::kEnabledStandardAvailableEnhanced;
  } else if (is_sb_managed) {
    status = SafeBrowsingStatus::kDisabledByAdmin;
  } else if (enabled_pref->IsExtensionControlled()) {
    status = SafeBrowsingStatus::kDisabledByExtension;
  } else {
    status = SafeBrowsingStatus::kDisabled;
  }
  handler_->OnSafeBrowsingCheckResult(status);
}

}  // namespace safety_check
