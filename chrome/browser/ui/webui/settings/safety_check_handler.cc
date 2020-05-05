// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

// Constants for communication with JS.
constexpr char kParentEvent[] = "safety-check-parent-status-changed";
constexpr char kUpdatesEvent[] = "safety-check-updates-status-changed";
constexpr char kPasswordsEvent[] = "safety-check-passwords-status-changed";
constexpr char kSafeBrowsingEvent[] =
    "safety-check-safe-browsing-status-changed";
constexpr char kExtensionsEvent[] = "safety-check-extensions-status-changed";
constexpr char kPerformSafetyCheck[] = "performSafetyCheck";
constexpr char kGetParentRanDisplayString[] = "getSafetyCheckRanDisplayString";
constexpr char kNewState[] = "newState";
constexpr char kDisplayString[] = "displayString";
constexpr char kPasswordsCompromised[] = "passwordsCompromised";
constexpr char kExtensionsReenabledByUser[] = "extensionsReenabledByUser";
constexpr char kExtensionsReenabledByAdmin[] = "extensionsReenabledByAdmin";

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
    case VersionUpdater::DISABLED_BY_ADMIN:
      return SafetyCheckHandler::UpdateStatus::kDisabledByAdmin;
    // The disabled state can only be returned on non Chrome-branded browsers.
    case VersionUpdater::DISABLED:
      return SafetyCheckHandler::UpdateStatus::kUnknown;
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

void SafetyCheckHandler::SendSafetyCheckStartedWebUiUpdates() {
  AllowJavascript();

  // Reset status of parent and children, which might have been set from a
  // previous run of safety check.
  parent_status_ = ParentStatus::kChecking;
  update_status_ = UpdateStatus::kChecking;
  passwords_status_ = PasswordsStatus::kChecking;
  safe_browsing_status_ = SafeBrowsingStatus::kChecking;
  extensions_status_ = ExtensionsStatus::kChecking;

  // Update WebUi.
  FireBasicSafetyCheckWebUiListener(kParentEvent,
                                    static_cast<int>(parent_status_),
                                    GetStringForParent(parent_status_));
  FireBasicSafetyCheckWebUiListener(kUpdatesEvent,
                                    static_cast<int>(update_status_),
                                    GetStringForUpdates(update_status_));
  FireBasicSafetyCheckWebUiListener(
      kPasswordsEvent, static_cast<int>(passwords_status_),
      GetStringForPasswords(passwords_status_, Compromised(0), Done(0),
                            Total(0)));
  FireBasicSafetyCheckWebUiListener(
      kSafeBrowsingEvent, static_cast<int>(safe_browsing_status_),
      GetStringForSafeBrowsing(safe_browsing_status_));
  FireBasicSafetyCheckWebUiListener(
      kExtensionsEvent, static_cast<int>(extensions_status_),
      GetStringForExtensions(extensions_status_, Blocklisted(0),
                             ReenabledUser(0), ReenabledAdmin(0)));
}

void SafetyCheckHandler::PerformSafetyCheck() {
  // Checks common to desktop, Android, and iOS are handled by
  // safety_check::SafetyCheck.
  safety_check_.reset(new safety_check::SafetyCheck(this));
  safety_check_->CheckSafeBrowsing(Profile::FromWebUI(web_ui())->GetPrefs());

  if (!version_updater_) {
    version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  }
  DCHECK(version_updater_);
  CheckUpdates();

  if (!leak_service_) {
    leak_service_ = BulkLeakCheckServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui()));
  }
  DCHECK(leak_service_);
  if (!passwords_delegate_) {
    passwords_delegate_ =
        extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
            Profile::FromWebUI(web_ui()), true);
  }
  DCHECK(passwords_delegate_);
  CheckPasswords();

  if (!extension_prefs_) {
    extension_prefs_ = extensions::ExtensionPrefsFactory::GetForBrowserContext(
        Profile::FromWebUI(web_ui()));
  }
  DCHECK(extension_prefs_);
  if (!extension_service_) {
    extension_service_ =
        extensions::ExtensionSystem::Get(Profile::FromWebUI(web_ui()))
            ->extension_service();
  }
  DCHECK(extension_service_);
  CheckExtensions();
}

SafetyCheckHandler::SafetyCheckHandler(
    std::unique_ptr<VersionUpdater> version_updater,
    password_manager::BulkLeakCheckService* leak_service,
    extensions::PasswordsPrivateDelegate* passwords_delegate,
    extensions::ExtensionPrefs* extension_prefs,
    extensions::ExtensionServiceInterface* extension_service)
    : version_updater_(std::move(version_updater)),
      leak_service_(leak_service),
      passwords_delegate_(passwords_delegate),
      extension_prefs_(extension_prefs),
      extension_service_(extension_service) {}

void SafetyCheckHandler::HandlePerformSafetyCheck(const base::ListValue* args) {
  SendSafetyCheckStartedWebUiUpdates();

  // Run safety check after a delay. This ensures that the "running" state is
  // visible to users for each safety check child, even if a child would
  // otherwise complete in an instant.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SafetyCheckHandler::PerformSafetyCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void SafetyCheckHandler::HandleGetParentRanDisplayString(
    const base::ListValue* args) {
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(
      *callback_id,
      base::Value(GetStringForParentRan(safety_check_completion_time_)));
}

void SafetyCheckHandler::CheckUpdates() {
  // Usage of base::Unretained(this) is safe, because we own `version_updater_`.
  version_updater_->CheckForUpdate(
      base::Bind(&SafetyCheckHandler::OnUpdateCheckResult,
                 base::Unretained(this)),
      VersionUpdater::PromoteCallback());
}

void SafetyCheckHandler::CheckPasswords() {
  // Remove |this| as an existing observer for BulkLeakCheck if it is
  // registered. This takes care of an edge case when safety check starts twice
  // on the same page. Normally this should not happen, but if it does, the
  // browser should not crash.
  observed_leak_check_.RemoveAll();
  observed_leak_check_.Add(leak_service_);
  passwords_delegate_->StartPasswordCheck(base::BindOnce(
      &SafetyCheckHandler::OnStateChanged, weak_ptr_factory_.GetWeakPtr()));
}

void SafetyCheckHandler::CheckExtensions() {
  extensions::ExtensionIdList extensions;
  extension_prefs_->GetExtensions(&extensions);
  int blocklisted = 0;
  int reenabled_by_user = 0;
  int reenabled_by_admin = 0;
  for (auto extension_id : extensions) {
    extensions::BlacklistState state =
        extension_prefs_->GetExtensionBlacklistState(extension_id);
    if (state == extensions::BLACKLISTED_UNKNOWN) {
      // If any of the extensions are in the unknown blacklist state, that means
      // there was an error the last time the blacklist was fetched. That means
      // the results cannot be relied upon.
      OnExtensionsCheckResult(ExtensionsStatus::kError, Blocklisted(0),
                              ReenabledUser(0), ReenabledAdmin(0));
      return;
    }
    if (state == extensions::NOT_BLACKLISTED) {
      continue;
    }
    ++blocklisted;
    if (!extension_service_->IsExtensionEnabled(extension_id)) {
      continue;
    }
    if (extension_service_->UserCanDisableInstalledExtension(extension_id)) {
      ++reenabled_by_user;
    } else {
      ++reenabled_by_admin;
    }
  }
  if (blocklisted == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kNoneBlocklisted, Blocklisted(0),
                            ReenabledUser(0), ReenabledAdmin(0));
  } else if (reenabled_by_user == 0 && reenabled_by_admin == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedAllDisabled,
                            Blocklisted(blocklisted), ReenabledUser(0),
                            ReenabledAdmin(0));
  } else if (reenabled_by_user > 0 && reenabled_by_admin == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedReenabledAllByUser,
                            Blocklisted(blocklisted),
                            ReenabledUser(reenabled_by_user),
                            ReenabledAdmin(0));
  } else if (reenabled_by_admin > 0 && reenabled_by_user == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedReenabledAllByAdmin,
                            Blocklisted(blocklisted), ReenabledUser(0),
                            ReenabledAdmin(reenabled_by_admin));
  } else {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedReenabledSomeByUser,
                            Blocklisted(blocklisted),
                            ReenabledUser(reenabled_by_user),
                            ReenabledAdmin(reenabled_by_admin));
  }
}

void SafetyCheckHandler::OnUpdateCheckResult(VersionUpdater::Status status,
                                             int progress,
                                             bool rollback,
                                             const std::string& version,
                                             int64_t update_size,
                                             const base::string16& message) {
  update_status_ = ConvertToUpdateStatus(status);
  if (update_status_ != UpdateStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.UpdatesResult",
                                  update_status_);
  }
  // TODO(crbug/1072432): Since the UNKNOWN state is not present in JS in M83,
  // use FAILED_OFFLINE, which uses the same icon.
  FireBasicSafetyCheckWebUiListener(
      kUpdatesEvent,
      static_cast<int>(update_status_ != UpdateStatus::kUnknown
                           ? update_status_
                           : UpdateStatus::kFailedOffline),
      GetStringForUpdates(update_status_));
  CompleteParentIfChildrenCompleted();
}

void SafetyCheckHandler::OnPasswordsCheckResult(PasswordsStatus status,
                                                Compromised compromised,
                                                Done done,
                                                Total total) {
  base::DictionaryValue event;
  event.SetIntKey(kNewState, static_cast<int>(status));
  if (status == PasswordsStatus::kCompromisedExist) {
    event.SetIntKey(kPasswordsCompromised, compromised.value());
  }
  event.SetStringKey(kDisplayString,
                     GetStringForPasswords(status, compromised, done, total));
  FireWebUIListener(kPasswordsEvent, event);
  if (status != PasswordsStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.PasswordsResult",
                                  status);
  }
  passwords_status_ = status;
  CompleteParentIfChildrenCompleted();
}

void SafetyCheckHandler::OnExtensionsCheckResult(
    ExtensionsStatus status,
    Blocklisted blocklisted,
    ReenabledUser reenabled_user,
    ReenabledAdmin reenabled_admin) {
  base::DictionaryValue event;
  event.SetIntKey(kNewState, static_cast<int>(status));
  if (status == ExtensionsStatus::kBlocklistedReenabledAllByUser ||
      status == ExtensionsStatus::kBlocklistedReenabledSomeByUser) {
    event.SetIntKey(kExtensionsReenabledByUser, reenabled_user.value());
  }
  if (status == ExtensionsStatus::kBlocklistedReenabledAllByAdmin ||
      status == ExtensionsStatus::kBlocklistedReenabledSomeByUser) {
    event.SetIntKey(kExtensionsReenabledByAdmin, reenabled_admin.value());
  }
  event.SetStringKey(kDisplayString,
                     GetStringForExtensions(status, Blocklisted(blocklisted),
                                            reenabled_user, reenabled_admin));
  FireWebUIListener(kExtensionsEvent, event);
  if (status != ExtensionsStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.ExtensionsResult",
                                  status);
  }
  extensions_status_ = status;
  CompleteParentIfChildrenCompleted();
}

base::string16 SafetyCheckHandler::GetStringForParent(ParentStatus status) {
  switch (status) {
    case ParentStatus::kBefore:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_BEFORE);
    case ParentStatus::kChecking:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_CHECK_RUNNING);
    case ParentStatus::kAfter:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER);
  }
}

base::string16 SafetyCheckHandler::GetStringForUpdates(UpdateStatus status) {
  switch (status) {
    case UpdateStatus::kChecking:
      return base::UTF8ToUTF16("");
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
    case UpdateStatus::kUnknown:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ABOUT_PAGE_BROWSER_VERSION,
          base::UTF8ToUTF16(version_info::GetVersionNumber()),
          l10n_util::GetStringUTF16(version_info::IsOfficialBuild()
                                        ? IDS_VERSION_UI_OFFICIAL
                                        : IDS_VERSION_UI_UNOFFICIAL),
          base::UTF8ToUTF16(chrome::GetChannelName()),
          l10n_util::GetStringUTF16(sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT
                                                       : IDS_VERSION_UI_32BIT));
  }
}

base::string16 SafetyCheckHandler::GetStringForSafeBrowsing(
    SafeBrowsingStatus status) {
  switch (status) {
    case SafeBrowsingStatus::kChecking:
      return base::UTF8ToUTF16("");
    case SafeBrowsingStatus::kEnabled:
    case SafeBrowsingStatus::kEnabledStandard:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENABLED_STANDARD);
    case SafeBrowsingStatus::kEnabledEnhanced:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENABLED_ENHANCED);
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

base::string16 SafetyCheckHandler::GetStringForPasswords(
    PasswordsStatus status,
    Compromised compromised,
    Done done,
    Total total) {
  switch (status) {
    case PasswordsStatus::kChecking: {
      // Unable to get progress for some reason.
      if (total.value() == 0) {
        return base::UTF8ToUTF16("");
      }
      return l10n_util::GetStringFUTF16(IDS_SETTINGS_CHECK_PASSWORDS_PROGRESS,
                                        base::FormatNumber(done.value()),
                                        base::FormatNumber(total.value()));
    }
    case PasswordsStatus::kSafe:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT, 0);
    case PasswordsStatus::kCompromisedExist:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT, compromised.value());
    case PasswordsStatus::kOffline:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_OFFLINE);
    case PasswordsStatus::kNoPasswords:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_NO_PASSWORDS);
    case PasswordsStatus::kSignedOut:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_SIGNED_OUT);
    case PasswordsStatus::kQuotaLimit:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_QUOTA_LIMIT);
    case PasswordsStatus::kError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_GENERIC);
  }
}

base::string16 SafetyCheckHandler::GetStringForExtensions(
    ExtensionsStatus status,
    Blocklisted blocklisted,
    ReenabledUser reenabled_user,
    ReenabledAdmin reenabled_admin) {
  switch (status) {
    case ExtensionsStatus::kChecking:
      return base::UTF8ToUTF16("");
    case ExtensionsStatus::kError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_ERROR);
    case ExtensionsStatus::kNoneBlocklisted:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_SAFE);
    case ExtensionsStatus::kBlocklistedAllDisabled:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_OFF,
          blocklisted.value());
    case ExtensionsStatus::kBlocklistedReenabledAllByUser:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_USER,
          reenabled_user.value());
    case ExtensionsStatus::kBlocklistedReenabledSomeByUser:
      // TODO(crbug/1060625): Make string concatenation with a period
      // internationalized (see go/i18n-concatenation).
      return l10n_util::GetPluralStringFUTF16(
                 IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_USER,
                 reenabled_user.value()) +
             base::ASCIIToUTF16(". ") +
             l10n_util::GetPluralStringFUTF16(
                 IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_ADMIN,
                 reenabled_admin.value()) +
             base::ASCIIToUTF16(".");
    case ExtensionsStatus::kBlocklistedReenabledAllByAdmin:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_ADMIN,
          reenabled_admin.value());
  }
}

base::string16 SafetyCheckHandler::GetStringForParentRan(
    base::Time safety_check_completion_time) {
  return SafetyCheckHandler::GetStringForParentRan(safety_check_completion_time,
                                                   base::Time::Now());
}

base::string16 SafetyCheckHandler::GetStringForParentRan(
    base::Time safety_check_completion_time,
    base::Time system_time) {
  base::Time::Exploded completion_time_exploded;
  safety_check_completion_time.LocalExplode(&completion_time_exploded);

  base::Time::Exploded system_time_exploded;
  system_time.LocalExplode(&system_time_exploded);

  const base::Time time_yesterday = system_time - base::TimeDelta::FromDays(1);
  base::Time::Exploded time_yesterday_exploded;
  time_yesterday.LocalExplode(&time_yesterday_exploded);

  const auto time_diff = system_time - safety_check_completion_time;
  if (completion_time_exploded.year == system_time_exploded.year &&
      completion_time_exploded.month == system_time_exploded.month &&
      completion_time_exploded.day_of_month ==
          system_time_exploded.day_of_month) {
    // Safety check ran today.
    const int time_diff_in_mins = time_diff.InMinutes();
    if (time_diff_in_mins == 0) {
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER);
    } else if (time_diff_in_mins < 60) {
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_MINS,
          time_diff_in_mins);
    } else {
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_HOURS,
          time_diff_in_mins / 60);
    }
  } else if (completion_time_exploded.year == time_yesterday_exploded.year &&
             completion_time_exploded.month == time_yesterday_exploded.month &&
             completion_time_exploded.day_of_month ==
                 time_yesterday_exploded.day_of_month) {
    // Safety check ran yesterday.
    return l10n_util::GetStringUTF16(
        IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_YESTERDAY);
  } else {
    // Safety check ran longer ago than yesterday.
    // TODO(crbug.com/1015841): While a minor issue, this is not be the ideal
    // way to calculate the days passed since safety check ran. For example,
    // <48 h might still be 2 days ago.
    const int time_diff_in_days = time_diff.InDays();
    return l10n_util::GetPluralStringFUTF16(
        IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_DAYS,
        time_diff_in_days);
  }
}

void SafetyCheckHandler::DetermineIfNoPasswordsOrSafe(
    const std::vector<extensions::api::passwords_private::PasswordUiEntry>&
        passwords) {
  OnPasswordsCheckResult(passwords.empty() ? PasswordsStatus::kNoPasswords
                                           : PasswordsStatus::kSafe,
                         Compromised(0), Done(0), Total(0));
}

void SafetyCheckHandler::OnSafeBrowsingCheckResult(
    SafetyCheckHandler::SafeBrowsingStatus status) {
  safe_browsing_status_ = status;
  if (safe_browsing_status_ != SafeBrowsingStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.SafeBrowsingResult",
                                  safe_browsing_status_);
  }
  FireBasicSafetyCheckWebUiListener(
      kSafeBrowsingEvent, static_cast<int>(safe_browsing_status_),
      GetStringForSafeBrowsing(safe_browsing_status_));
  CompleteParentIfChildrenCompleted();
}

void SafetyCheckHandler::OnStateChanged(
    password_manager::BulkLeakCheckService::State state) {
  using password_manager::BulkLeakCheckService;
  switch (state) {
    case BulkLeakCheckService::State::kIdle:
    case BulkLeakCheckService::State::kCanceled: {
      size_t num_compromised =
          passwords_delegate_->GetCompromisedCredentials().size();
      if (num_compromised == 0) {
        passwords_delegate_->GetSavedPasswordsList(
            base::BindOnce(&SafetyCheckHandler::DetermineIfNoPasswordsOrSafe,
                           base::Unretained(this)));
      } else {
        OnPasswordsCheckResult(PasswordsStatus::kCompromisedExist,
                               Compromised(num_compromised), Done(0), Total(0));
      }
      break;
    }
    case BulkLeakCheckService::State::kRunning:
      OnPasswordsCheckResult(PasswordsStatus::kChecking, Compromised(0),
                             Done(0), Total(0));
      // Non-terminal state, so nothing else needs to be done.
      return;
    case BulkLeakCheckService::State::kSignedOut:
      OnPasswordsCheckResult(PasswordsStatus::kSignedOut, Compromised(0),
                             Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kNetworkError:
      OnPasswordsCheckResult(PasswordsStatus::kOffline, Compromised(0), Done(0),
                             Total(0));
      break;
    case BulkLeakCheckService::State::kQuotaLimit:
      OnPasswordsCheckResult(PasswordsStatus::kQuotaLimit, Compromised(0),
                             Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kTokenRequestFailure:
    case BulkLeakCheckService::State::kHashingFailure:
    case BulkLeakCheckService::State::kServiceError:
      OnPasswordsCheckResult(PasswordsStatus::kError, Compromised(0), Done(0),
                             Total(0));
      break;
  }

  // Stop observing the leak service in all terminal states, if it's still being
  // observed.
  observed_leak_check_.RemoveAll();
}

void SafetyCheckHandler::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  extensions::api::passwords_private::PasswordCheckStatus status =
      passwords_delegate_->GetPasswordCheckStatus();
  // Send progress updates only if the check is still running.
  if (status.state ==
          extensions::api::passwords_private::PASSWORD_CHECK_STATE_RUNNING &&
      status.already_processed && status.remaining_in_queue) {
    Done done = Done(*(status.already_processed));
    Total total = Total(*(status.remaining_in_queue) + done.value());
    OnPasswordsCheckResult(PasswordsStatus::kChecking, Compromised(0), done,
                           total);
  }
}

void SafetyCheckHandler::OnJavascriptAllowed() {}

void SafetyCheckHandler::OnJavascriptDisallowed() {
  // Remove |this| as an observer for BulkLeakCheck. This takes care of an edge
  // case when the page is reloaded while the password check is in progress and
  // another safety check is started. Otherwise |observed_leak_check_|
  // automatically calls RemoveAll() on destruction.
  observed_leak_check_.RemoveAll();
  // Destroy the version updater to prevent getting a callback and firing a
  // WebUI event, which would cause a crash.
  version_updater_.reset();
  // Stop observing safety check events.
  safety_check_.reset(nullptr);
}

void SafetyCheckHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      kPerformSafetyCheck,
      base::BindRepeating(&SafetyCheckHandler::HandlePerformSafetyCheck,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kGetParentRanDisplayString,
      base::BindRepeating(&SafetyCheckHandler::HandleGetParentRanDisplayString,
                          base::Unretained(this)));
}

void SafetyCheckHandler::CompleteParentIfChildrenCompleted() {
  if (update_status_ != UpdateStatus::kChecking &&
      passwords_status_ != PasswordsStatus::kChecking &&
      safe_browsing_status_ != SafeBrowsingStatus::kChecking &&
      extensions_status_ != ExtensionsStatus::kChecking) {
    parent_status_ = ParentStatus::kAfter;
    // Remember when safety check completed.
    safety_check_completion_time_ = base::Time::Now();
    // Update UI.
    FireBasicSafetyCheckWebUiListener(kParentEvent,
                                      static_cast<int>(parent_status_),
                                      GetStringForParent(parent_status_));
  }
}

void SafetyCheckHandler::FireBasicSafetyCheckWebUiListener(
    const std::string& event_name,
    int new_state,
    const base::string16& display_string) {
  base::DictionaryValue event;
  event.SetIntKey(kNewState, new_state);
  event.SetStringKey(kDisplayString, display_string);
  FireWebUIListener(event_name, event);
}
