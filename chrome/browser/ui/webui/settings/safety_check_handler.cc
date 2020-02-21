// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/safety_check_handler_observer.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {

// Constants for communication with JS.
static constexpr char kStatusChanged[] = "safety-check-status-changed";
static constexpr char kInitialize[] = "initializeSafetyCheck";
static constexpr char kPerformSafetyCheck[] = "performSafetyCheck";
static constexpr char kSafetyCheckComponent[] = "safetyCheckComponent";
static constexpr char kNewState[] = "newState";

// Converts the VersionUpdater::Status to the UpdateStatus enum to be passed
// to the safety check frontend. Note: if the VersionUpdater::Status gets
// changed, this will fail to compile. That is done intentionally to ensure
// that the states of the safety check are always in sync with the
// VersionUpdater ones.
SafetyCheckHandler::UpdateStatus ConvertToUpdateStatus(
    VersionUpdater::Status status) {
  switch (status) {
    case VersionUpdater::CHECKING:
      return SafetyCheckHandler::UpdateStatus::kChecking;
    case VersionUpdater::UPDATED:
      return SafetyCheckHandler::UpdateStatus::kUpdated;
    case VersionUpdater::UPDATING:
      return SafetyCheckHandler::UpdateStatus::kUpdating;
    case VersionUpdater::NEED_PERMISSION_TO_UPDATE:
    case VersionUpdater::NEARLY_UPDATED:
      return SafetyCheckHandler::UpdateStatus::kRelaunch;
    case VersionUpdater::DISABLED:
    case VersionUpdater::DISABLED_BY_ADMIN:
      return SafetyCheckHandler::UpdateStatus::kDisabledByAdmin;
    case VersionUpdater::FAILED:
    case VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED:
      return SafetyCheckHandler::UpdateStatus::kFailed;
    case VersionUpdater::FAILED_OFFLINE:
      return SafetyCheckHandler::UpdateStatus::kFailedOffline;
  }
}
}  // namespace

SafetyCheckHandler::SafetyCheckHandler()
    : SafetyCheckHandler(nullptr, nullptr) {}

SafetyCheckHandler::~SafetyCheckHandler() = default;

void SafetyCheckHandler::PerformSafetyCheck() {
  if (!version_updater_) {
    version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  }
  CheckUpdates();
  CheckSafeBrowsing();
}

SafetyCheckHandler::SafetyCheckHandler(
    std::unique_ptr<VersionUpdater> version_updater,
    SafetyCheckHandlerObserver* observer)
    : version_updater_(std::move(version_updater)), observer_(observer) {}

void SafetyCheckHandler::HandleInitialize(const base::ListValue* /*args*/) {
  AllowJavascript();
}

void SafetyCheckHandler::HandlePerformSafetyCheck(
    const base::ListValue* /*args*/) {
  PerformSafetyCheck();
}

void SafetyCheckHandler::CheckUpdates() {
  if (observer_) {
    observer_->OnUpdateCheckStart();
  }
  version_updater_->CheckForUpdate(
      base::Bind(&SafetyCheckHandler::OnUpdateCheckResult,
                 base::Unretained(this)),
      VersionUpdater::PromoteCallback());
}

void SafetyCheckHandler::CheckSafeBrowsing() {
  if (observer_) {
    observer_->OnSafeBrowsingCheckStart();
  }
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kSafeBrowsingEnabled);
  SafeBrowsingStatus status;
  if (pref_service->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    status = SafeBrowsingStatus::kEnabled;
  } else if (pref->IsManaged()) {
    status = SafeBrowsingStatus::kDisabledByAdmin;
  } else if (pref->IsExtensionControlled()) {
    status = SafeBrowsingStatus::kDisabledByExtension;
  } else {
    status = SafeBrowsingStatus::kDisabled;
  }
  OnSafeBrowsingCheckResult(status);
}

void SafetyCheckHandler::OnUpdateCheckResult(
    VersionUpdater::Status status,
    int /*progress*/,
    bool /*rollback*/,
    const std::string& /*version*/,
    int64_t /*update_size*/,
    const base::string16& /*message*/) {
  UpdateStatus update_status = ConvertToUpdateStatus(status);
  if (observer_) {
    observer_->OnUpdateCheckResult(update_status);
  }
  auto event = std::make_unique<base::DictionaryValue>();
  event->SetInteger(kSafetyCheckComponent,
                    static_cast<int>(SafetyCheckComponent::kUpdates));
  event->SetInteger(kNewState, static_cast<int>(update_status));
  FireWebUIListener(kStatusChanged, *event);
}

void SafetyCheckHandler::OnSafeBrowsingCheckResult(
    SafetyCheckHandler::SafeBrowsingStatus status) {
  if (observer_) {
    observer_->OnSafeBrowsingCheckResult(status);
  }
  auto event = std::make_unique<base::DictionaryValue>();
  event->SetInteger(kSafetyCheckComponent,
                    static_cast<int>(SafetyCheckComponent::kSafeBrowsing));
  event->SetInteger(kNewState, static_cast<int>(status));
  FireWebUIListener(kStatusChanged, *event);
}

void SafetyCheckHandler::OnJavascriptAllowed() {}

void SafetyCheckHandler::OnJavascriptDisallowed() {}

void SafetyCheckHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kInitialize, base::BindRepeating(&SafetyCheckHandler::HandleInitialize,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kPerformSafetyCheck,
      base::BindRepeating(&SafetyCheckHandler::HandlePerformSafetyCheck,
                          base::Unretained(this)));
}
