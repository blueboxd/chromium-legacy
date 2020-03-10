// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

// Constants for communication with JS.
constexpr char kUpdatesEvent[] = "safety-check-updates-status-changed";
constexpr char kPasswordsEvent[] = "safety-check-passwords-status-changed";
constexpr char kSafeBrowsingEvent[] =
    "safety-check-safe-browsing-status-changed";
constexpr char kPerformSafetyCheck[] = "performSafetyCheck";
constexpr char kNewState[] = "newState";
constexpr char kDisplayedString[] = "displayedString";
constexpr char kPasswordsCompromised[] = "passwordsCompromised";

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

SafetyCheckHandler::SafetyCheckHandler() = default;

SafetyCheckHandler::~SafetyCheckHandler() = default;

void SafetyCheckHandler::PerformSafetyCheck() {
  AllowJavascript();
  base::RecordAction(base::UserMetricsAction("SafetyCheck.Started"));
  if (!version_updater_) {
    version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  }
  CheckUpdates();
  CheckSafeBrowsing();
  if (!leak_service_) {
    leak_service_ = BulkLeakCheckServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui()));
  }
  DCHECK(leak_service_);
  CheckPasswords();
}

SafetyCheckHandler::SafetyCheckHandler(
    std::unique_ptr<VersionUpdater> version_updater,
    password_manager::BulkLeakCheckService* leak_service)
    : version_updater_(std::move(version_updater)),
      leak_service_(leak_service) {}

void SafetyCheckHandler::HandlePerformSafetyCheck(
    const base::ListValue* /*args*/) {
  PerformSafetyCheck();
}

void SafetyCheckHandler::CheckUpdates() {
  version_updater_->CheckForUpdate(
      base::Bind(&SafetyCheckHandler::OnUpdateCheckResult,
                 base::Unretained(this)),
      VersionUpdater::PromoteCallback());
}

void SafetyCheckHandler::CheckSafeBrowsing() {
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

void SafetyCheckHandler::CheckPasswords() {
  // Remove |this| as an existing observer for BulkLeakCheck if it is
  // registered. This takes care of an edge case when safety check starts twice
  // on the same page. Normally this should not happen, but if it does, the
  // browser should not crash.
  observed_leak_check_.RemoveAll();
  observed_leak_check_.Add(leak_service_);
  // TODO(crbug.com/1015841): Implement starting a leak check if one is not
  // running already once the API for it becomes available (see
  // crrev.com/c/2072742 and follow up CLs).
}

void SafetyCheckHandler::OnUpdateCheckResult(VersionUpdater::Status status,
                                             int progress,
                                             bool rollback,
                                             const std::string& version,
                                             int64_t update_size,
                                             const base::string16& message) {
  UpdateStatus update_status = ConvertToUpdateStatus(status);
  base::DictionaryValue event;
  event.SetIntKey(kNewState, static_cast<int>(update_status));
  event.SetStringKey(kDisplayedString, GetStringForUpdates(update_status));
  FireWebUIListener(kUpdatesEvent, event);
}

void SafetyCheckHandler::OnSafeBrowsingCheckResult(
    SafetyCheckHandler::SafeBrowsingStatus status) {
  base::DictionaryValue event;
  event.SetIntKey(kNewState, static_cast<int>(status));
  event.SetStringKey(kDisplayedString, GetStringForSafeBrowsing(status));
  FireWebUIListener(kSafeBrowsingEvent, event);
}

void SafetyCheckHandler::OnPasswordsCheckResult(PasswordsStatus status,
                                                int num_compromised) {
  base::DictionaryValue event;
  event.SetIntKey(kNewState, static_cast<int>(status));
  if (status == PasswordsStatus::kCompromisedExist) {
    event.SetIntKey(kPasswordsCompromised, num_compromised);
  }
  event.SetStringKey(kDisplayedString,
                     GetStringForPasswords(status, num_compromised));
  FireWebUIListener(kPasswordsEvent, event);
}

base::string16 SafetyCheckHandler::GetStringForUpdates(UpdateStatus status) {
  switch (status) {
    case UpdateStatus::kChecking:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_CHECK_RUNNING);
    case UpdateStatus::kUpdated:
#if defined(OS_CHROMEOS)
      return ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE);
#else
      return l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE);
#endif
    case UpdateStatus::kUpdating:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UPDATING);
    case UpdateStatus::kRelaunch:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH);
    case UpdateStatus::kDisabledByAdmin:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_DISABLED_BY_ADMIN,
          base::ASCIIToUTF16(chrome::kWhoIsMyAdministratorHelpURL));
    case UpdateStatus::kFailedOffline:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_FAILED_OFFLINE);
    case UpdateStatus::kFailed:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_FAILED,
          base::ASCIIToUTF16(chrome::kChromeFixUpdateProblems));
  }
}

base::string16 SafetyCheckHandler::GetStringForSafeBrowsing(
    SafeBrowsingStatus status) {
  switch (status) {
    case SafeBrowsingStatus::kChecking:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_CHECK_RUNNING);
    case SafeBrowsingStatus::kEnabled:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENABLED);
    case SafeBrowsingStatus::kDisabled:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED);
    case SafeBrowsingStatus::kDisabledByAdmin:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_BY_ADMIN,
          base::ASCIIToUTF16(chrome::kWhoIsMyAdministratorHelpURL));
    case SafeBrowsingStatus::kDisabledByExtension:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_BY_EXTENSION);
  }
}

base::string16 SafetyCheckHandler::GetStringForPasswords(PasswordsStatus status,
                                                         int num_compromised) {
  switch (status) {
    case PasswordsStatus::kChecking:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_CHECK_RUNNING);
    case PasswordsStatus::kSafe:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_SAFE);
    case PasswordsStatus::kCompromisedExist:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_COMPROMISED, num_compromised);
    case PasswordsStatus::kOffline:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_OFFLINE);
    case PasswordsStatus::kNoPasswords:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_NO_PASSWORDS);
    case PasswordsStatus::kSignedOut:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_SIGNED_OUT);
    case PasswordsStatus::kQuotaLimit:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_QUOTA_LIMIT);
    case PasswordsStatus::kTooManyPasswords:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_TOO_MANY_PASSWORDS);
    case PasswordsStatus::kError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_ERROR);
  }
}

void SafetyCheckHandler::OnStateChanged(
    password_manager::BulkLeakCheckService::State state) {
  using password_manager::BulkLeakCheckService;
  switch (state) {
    case BulkLeakCheckService::State::kIdle:
    case BulkLeakCheckService::State::kCanceled:
      // TODO(crbug.com/1015841): Implement retrieving the number
      // of leaked passwords (if any) once PasswordsPrivateDelegate provides an
      // API for that (see crrev.com/c/2072742).
      break;
    case BulkLeakCheckService::State::kRunning:
      OnPasswordsCheckResult(PasswordsStatus::kChecking, 0);
      return;
    case BulkLeakCheckService::State::kSignedOut:
      OnPasswordsCheckResult(PasswordsStatus::kSignedOut, 0);
      break;
    case BulkLeakCheckService::State::kNetworkError:
      OnPasswordsCheckResult(PasswordsStatus::kOffline, 0);
      break;
    case BulkLeakCheckService::State::kTokenRequestFailure:
    case BulkLeakCheckService::State::kHashingFailure:
    case BulkLeakCheckService::State::kServiceError:
      OnPasswordsCheckResult(PasswordsStatus::kError, 0);
      break;
  }
  // TODO(crbug.com/1015841): implement detecting the following states if it is
  // possible: kNoPasswords, kQuotaLimit, and kTooManyPasswords.

  // Stop observing the leak service in all terminal states.
  observed_leak_check_.Remove(leak_service_);
}

void SafetyCheckHandler::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  // Do nothing because we only want to know the total number of compromised
  // credentials at the end of the bulk leak check.
}

void SafetyCheckHandler::OnJavascriptAllowed() {}

void SafetyCheckHandler::OnJavascriptDisallowed() {
  // Remove |this| as an observer for BulkLeakCheck. This takes care of an edge
  // case when the page is reloaded while the password check is in progress and
  // another safety check is started. Otherwise |observed_leak_check_|
  // automatically calls RemoveAll() on destruction.
  observed_leak_check_.RemoveAll();
}

void SafetyCheckHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kPerformSafetyCheck,
      base::BindRepeating(&SafetyCheckHandler::HandlePerformSafetyCheck,
                          base::Unretained(this)));
}
