// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"

#include "chrome/browser/chromeos/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

FamilyUserMetricsService::FamilyUserMetricsService(
    content::BrowserContext* context) {
  PrefService* pref_service = Profile::FromBrowserContext(context)->GetPrefs();
  family_user_session_metrics_ =
      std::make_unique<FamilyUserSessionMetrics>(pref_service);
}

FamilyUserMetricsService::~FamilyUserMetricsService() = default;

void FamilyUserMetricsService::Shutdown() {
  family_user_session_metrics_.reset();
}

}  // namespace chromeos
