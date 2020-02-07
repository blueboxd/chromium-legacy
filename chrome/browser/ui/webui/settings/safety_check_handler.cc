// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/safety_check_handler_observer.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

SafetyCheckHandler::SafetyCheckHandler()
    : SafetyCheckHandler(nullptr, nullptr) {}

SafetyCheckHandler::~SafetyCheckHandler() = default;

void SafetyCheckHandler::PerformSafetyCheck() {
  if (!version_updater_) {
    version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  }
  CheckUpdates(base::Bind(&SafetyCheckHandler::OnUpdateCheckResult,
                          base::Unretained(this)));
  CheckSafeBrowsing();
}

void SafetyCheckHandler::OnJavascriptAllowed() {}

void SafetyCheckHandler::OnJavascriptDisallowed() {}

void SafetyCheckHandler::RegisterMessages() {}

SafetyCheckHandler::SafetyCheckHandler(
    std::unique_ptr<VersionUpdater> version_updater,
    SafetyCheckHandlerObserver* observer)
    : version_updater_(std::move(version_updater)), observer_(observer) {}

void SafetyCheckHandler::CheckUpdates(
    const VersionUpdater::StatusCallback& update_callback) {
  if (observer_) {
    observer_->OnUpdateCheckStart();
  }
  version_updater_->CheckForUpdate(update_callback,
                                   VersionUpdater::PromoteCallback());
}

void SafetyCheckHandler::CheckSafeBrowsing() {
  if (observer_) {
    observer_->OnSafeBrowsingCheckStart();
  }
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  bool enabled = pref_service->GetBoolean(prefs::kSafeBrowsingEnabled);
  SafeBrowsingStatus status;
  if (!enabled) {
    bool disabled_by_admin =
        pref_service->IsManagedPreference(prefs::kSafeBrowsingEnabled);
    status = disabled_by_admin ? SafeBrowsingStatus::DISABLED_BY_ADMIN
                               : SafeBrowsingStatus::DISABLED;
  } else {
    status = SafeBrowsingStatus::ENABLED;
  }
  OnSafeBrowsingCheckResult(status);
}

void SafetyCheckHandler::OnUpdateCheckResult(VersionUpdater::Status status,
                                             int progress,
                                             bool rollback,
                                             const std::string& version,
                                             int64_t update_size,
                                             const base::string16& message) {
  if (observer_) {
    observer_->OnUpdateCheckResult(status, progress, rollback, version,
                                   update_size, message);
  }
}

void SafetyCheckHandler::OnSafeBrowsingCheckResult(
    SafetyCheckHandler::SafeBrowsingStatus status) {
  if (observer_) {
    observer_->OnSafeBrowsingCheckResult(status);
  }
}
