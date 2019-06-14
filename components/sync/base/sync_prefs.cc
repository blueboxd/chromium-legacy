// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_buildflags.h"
#include "components/sync/base/pref_names.h"

namespace syncer {

namespace {

// Obsolete prefs related to the removed ClearServerData flow.
const char kSyncPassphraseEncryptionTransitionInProgress[] =
    "sync.passphrase_encryption_transition_in_progress";
const char kSyncNigoriStateForPassphraseTransition[] =
    "sync.nigori_state_for_passphrase_transition";

// Obsolete pref that used to store a bool on whether Sync has an auth error.
const char kSyncHasAuthError[] = "sync.has_auth_error";

// Obsolete pref that used to store the timestamp of first sync.
const char kSyncFirstSyncTime[] = "sync.first_sync_time";

// Obsolete pref that used to store long poll intervals received by the server.
const char kSyncLongPollIntervalSeconds[] = "sync.long_poll_interval";

#if defined(OS_CHROMEOS)
// Obsolete pref.
const char kSyncSpareBootstrapToken[] = "sync.spare_bootstrap_token";
#endif  // defined(OS_CHROMEOS)

std::vector<std::string> GetObsoleteUserTypePrefs() {
  return {prefs::kSyncAutofillProfile,
          prefs::kSyncAutofillWallet,
          prefs::kSyncAutofillWalletMetadata,
          prefs::kSyncSearchEngines,
          prefs::kSyncSessions,
          prefs::kSyncAppSettings,
          prefs::kSyncExtensionSettings,
          prefs::kSyncAppNotifications,
          prefs::kSyncHistoryDeleteDirectives,
          prefs::kSyncSyncedNotifications,
          prefs::kSyncSyncedNotificationAppInfo,
          prefs::kSyncDictionary,
          prefs::kSyncFaviconImages,
          prefs::kSyncFaviconTracking,
          prefs::kSyncDeviceInfo,
          prefs::kSyncPriorityPreferences,
          prefs::kSyncSupervisedUserSettings,
          prefs::kSyncSupervisedUsers,
          prefs::kSyncSupervisedUserSharedSettings,
          prefs::kSyncArticles,
          prefs::kSyncAppList,
          prefs::kSyncWifiCredentials,
          prefs::kSyncSupervisedUserWhitelists,
          prefs::kSyncArcPackage,
          prefs::kSyncUserEvents,
          prefs::kSyncMountainShares,
          prefs::kSyncUserConsents,
          prefs::kSyncSendTabToSelf};
}

void RegisterObsoleteUserTypePrefs(user_prefs::PrefRegistrySyncable* registry) {
  for (const std::string& obsolete_pref : GetObsoleteUserTypePrefs()) {
    registry->RegisterBooleanPref(obsolete_pref, false);
  }
}

const char* GetPrefNameForType(UserSelectableType type) {
  switch (type) {
    case UserSelectableType::kBookmarks:
      return prefs::kSyncBookmarks;
    case UserSelectableType::kPreferences:
      return prefs::kSyncPreferences;
    case UserSelectableType::kPasswords:
      return prefs::kSyncPasswords;
    case UserSelectableType::kAutofill:
      return prefs::kSyncAutofill;
    case UserSelectableType::kThemes:
      return prefs::kSyncThemes;
    case UserSelectableType::kHistory:
      // kSyncTypedUrls used here for historic reasons and pref backward
      // compatibility.
      return prefs::kSyncTypedUrls;
    case UserSelectableType::kExtensions:
      return prefs::kSyncExtensions;
    case UserSelectableType::kApps:
      return prefs::kSyncApps;
#if BUILDFLAG(ENABLE_READING_LIST)
    case UserSelectableType::kReadingList:
      return prefs::kSyncReadingList;
#endif
    case UserSelectableType::kTabs:
      return prefs::kSyncTabs;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

CryptoSyncPrefs::~CryptoSyncPrefs() {}

SyncPrefObserver::~SyncPrefObserver() {}

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::kSyncManaged, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncManagedPrefChanged,
                          base::Unretained(this)));
  pref_first_setup_complete_.Init(
      prefs::kSyncFirstSetupComplete, pref_service_,
      base::BindRepeating(&SyncPrefs::OnFirstSetupCompletePrefChange,
                          base::Unretained(this)));
  pref_sync_suppressed_.Init(
      prefs::kSyncSuppressStart, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncSuppressedPrefChange,
                          base::Unretained(this)));

  // Cache the value of the kEnableLocalSyncBackend pref to avoid it flipping
  // during the lifetime of the service.
  local_sync_enabled_ =
      pref_service_->GetBoolean(prefs::kEnableLocalSyncBackend);
}

SyncPrefs::~SyncPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void SyncPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Actual user-controlled preferences.
  registry->RegisterBooleanPref(prefs::kSyncFirstSetupComplete, false);
  registry->RegisterBooleanPref(prefs::kSyncSuppressStart, false);
  registry->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced, true);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    RegisterTypeSelectedPref(registry, type);
  }

  // Internal or bookkeeping prefs.
  registry->RegisterStringPref(prefs::kSyncCacheGuid, std::string());
  registry->RegisterStringPref(prefs::kSyncBirthday, std::string());
  registry->RegisterStringPref(prefs::kSyncBagOfChips, std::string());
  registry->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncLastPollTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncPollIntervalSeconds, 0);
  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());
  registry->RegisterStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                               std::string());
  registry->RegisterBooleanPref(prefs::kSyncPassphrasePrompted, false);
  registry->RegisterIntegerPref(prefs::kSyncMemoryPressureWarningCount, -1);
  registry->RegisterBooleanPref(prefs::kSyncShutdownCleanly, false);
  registry->RegisterDictionaryPref(prefs::kSyncInvalidationVersions);
  registry->RegisterStringPref(prefs::kSyncLastRunVersion, std::string());
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());

  // Obsolete prefs that will be removed after a grace period.
  RegisterObsoleteUserTypePrefs(registry);
  registry->RegisterBooleanPref(kSyncPassphraseEncryptionTransitionInProgress,
                                false);
  registry->RegisterStringPref(kSyncNigoriStateForPassphraseTransition,
                               std::string());
  registry->RegisterBooleanPref(kSyncHasAuthError, false);
  registry->RegisterInt64Pref(kSyncFirstSyncTime, 0);
  registry->RegisterInt64Pref(kSyncLongPollIntervalSeconds, 0);
#if defined(OS_CHROMEOS)
  registry->RegisterStringPref(kSyncSpareBootstrapToken, "");
#endif
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

void SyncPrefs::ClearPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ClearDirectoryConsistencyPreferences();

  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncLastPollTime);
  pref_service_->ClearPref(prefs::kSyncPollIntervalSeconds);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncPassphrasePrompted);
  pref_service_->ClearPref(prefs::kSyncMemoryPressureWarningCount);
  pref_service_->ClearPref(prefs::kSyncShutdownCleanly);
  pref_service_->ClearPref(prefs::kSyncInvalidationVersions);
  pref_service_->ClearPref(prefs::kSyncLastRunVersion);
  // No need to clear kManaged, kEnableLocalSyncBackend or kLocalSyncBackendDir,
  // since they're never actually set as user preferences.

  // Note: We do *not* clear prefs which are directly user-controlled such as
  // the set of selected types here, so that if the user ever chooses to enable
  // Sync again, they start off with their previous settings by default.
  // We do however require going through first-time setup again.
  pref_service_->ClearPref(prefs::kSyncFirstSetupComplete);
}

void SyncPrefs::ClearDirectoryConsistencyPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kSyncCacheGuid);
  pref_service_->ClearPref(prefs::kSyncBirthday);
  pref_service_->ClearPref(prefs::kSyncBagOfChips);
}

bool SyncPrefs::IsFirstSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncFirstSetupComplete);
}

void SyncPrefs::SetFirstSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncFirstSetupComplete, true);
}

bool SyncPrefs::IsSyncRequested() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // IsSyncRequested is the inverse of the old SuppressStart pref.
  // Since renaming a pref value is hard, here we still use the old one.
  return !pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetSyncRequested(bool is_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // See IsSyncRequested for why we use this pref and !is_requested.
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, !is_requested);
}

void SyncPrefs::SetSyncRequestedIfNotSetExplicitly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // GetUserPrefValue() returns nullptr if there is no user-set value for this
  // pref (there might still be a non-default value, e.g. from a policy, but we
  // explicitly don't care about that here).
  if (!pref_service_->GetUserPrefValue(prefs::kSyncSuppressStart)) {
    pref_service_->SetBoolean(prefs::kSyncSuppressStart, false);
  }
}

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastSyncedTime));
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
}

base::Time SyncPrefs::GetLastPollTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastPollTime));
}

void SyncPrefs::SetLastPollTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLastPollTime, time.ToInternalValue());
}

base::TimeDelta SyncPrefs::GetPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromSeconds(
      pref_service_->GetInt64(prefs::kSyncPollIntervalSeconds));
}

void SyncPrefs::SetPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncPollIntervalSeconds,
                          interval.InSeconds());
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

UserSelectableTypeSet SyncPrefs::GetSelectedTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return UserSelectableTypeSet::All();
  }

  UserSelectableTypeSet selected_types;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    const char* pref_name = GetPrefNameForType(type);
    DCHECK(pref_name);
    if (pref_service_->GetBoolean(pref_name)) {
      selected_types.Put(type);
    }
  }
  return selected_types;
}

void SyncPrefs::SetSelectedTypes(bool keep_everything_synced,
                                 UserSelectableTypeSet registered_types,
                                 UserSelectableTypeSet selected_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);

  for (UserSelectableType type : registered_types) {
    const char* pref_name = GetPrefNameForType(type);
    pref_service_->SetBoolean(pref_name, selected_types.Has(type));
  }

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}

bool SyncPrefs::IsManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken);
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
}

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken);
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
}

// static
const char* SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType type) {
  return GetPrefNameForType(type);
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
}

void SyncPrefs::OnFirstSetupCompletePrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnFirstSetupCompletePrefChange(*pref_first_setup_complete_);
}

void SyncPrefs::OnSyncSuppressedPrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note: The pref is inverted for historic reasons; see IsSyncRequested.
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncRequestedPrefChange(!*pref_sync_suppressed_);
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

// static
void SyncPrefs::RegisterTypeSelectedPref(
    user_prefs::PrefRegistrySyncable* registry,
    UserSelectableType type) {
  const char* pref_name = GetPrefNameForType(type);
  DCHECK(pref_name);
  registry->RegisterBooleanPref(pref_name, false);
}

void SyncPrefs::SetCacheGuid(const std::string& cache_guid) {
  pref_service_->SetString(prefs::kSyncCacheGuid, cache_guid);
}

std::string SyncPrefs::GetCacheGuid() const {
  return pref_service_->GetString(prefs::kSyncCacheGuid);
}

void SyncPrefs::SetBirthday(const std::string& birthday) {
  pref_service_->SetString(prefs::kSyncBirthday, birthday);
}

std::string SyncPrefs::GetBirthday() const {
  return pref_service_->GetString(prefs::kSyncBirthday);
}

void SyncPrefs::SetBagOfChips(const std::string& bag_of_chips) {
  // |bag_of_chips| contains a serialized proto which is not utf-8, hence we use
  // base64 encoding in prefs.
  std::string encoded;
  base::Base64Encode(bag_of_chips, &encoded);
  pref_service_->SetString(prefs::kSyncBagOfChips, encoded);
}

std::string SyncPrefs::GetBagOfChips() const {
  // |kSyncBagOfChips| gets stored in base64 because it represents a serialized
  // proto which is not utf-8 encoding.
  const std::string encoded = pref_service_->GetString(prefs::kSyncBagOfChips);
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

bool SyncPrefs::IsPassphrasePrompted() const {
  return pref_service_->GetBoolean(prefs::kSyncPassphrasePrompted);
}

void SyncPrefs::SetPassphrasePrompted(bool value) {
  pref_service_->SetBoolean(prefs::kSyncPassphrasePrompted, value);
}

int SyncPrefs::GetMemoryPressureWarningCount() const {
  return pref_service_->GetInteger(prefs::kSyncMemoryPressureWarningCount);
}

void SyncPrefs::SetMemoryPressureWarningCount(int value) {
  pref_service_->SetInteger(prefs::kSyncMemoryPressureWarningCount, value);
}

bool SyncPrefs::DidSyncShutdownCleanly() const {
  return pref_service_->GetBoolean(prefs::kSyncShutdownCleanly);
}

void SyncPrefs::SetCleanShutdown(bool value) {
  pref_service_->SetBoolean(prefs::kSyncShutdownCleanly, value);
}

void SyncPrefs::GetInvalidationVersions(
    std::map<ModelType, int64_t>* invalidation_versions) const {
  const base::DictionaryValue* invalidation_dictionary =
      pref_service_->GetDictionary(prefs::kSyncInvalidationVersions);
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    std::string key = ModelTypeToString(type);
    std::string version_str;
    if (!invalidation_dictionary->GetString(key, &version_str))
      continue;
    int64_t version = 0;
    if (!base::StringToInt64(version_str, &version))
      continue;
    (*invalidation_versions)[type] = version;
  }
}

void SyncPrefs::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  std::unique_ptr<base::DictionaryValue> invalidation_dictionary(
      new base::DictionaryValue());
  for (const auto& map_iter : invalidation_versions) {
    std::string version_str = base::NumberToString(map_iter.second);
    invalidation_dictionary->SetString(ModelTypeToString(map_iter.first),
                                       version_str);
  }
  pref_service_->Set(prefs::kSyncInvalidationVersions,
                     *invalidation_dictionary);
}

std::string SyncPrefs::GetLastRunVersion() const {
  return pref_service_->GetString(prefs::kSyncLastRunVersion);
}

void SyncPrefs::SetLastRunVersion(const std::string& current_version) {
  pref_service_->SetString(prefs::kSyncLastRunVersion, current_version);
}

bool SyncPrefs::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

void MigrateSessionsToProxyTabsPrefs(PrefService* pref_service) {
  if (pref_service->GetUserPrefValue(prefs::kSyncTabs) == nullptr &&
      pref_service->GetUserPrefValue(prefs::kSyncSessions) != nullptr &&
      pref_service->IsUserModifiablePreference(prefs::kSyncTabs)) {
    // If there is no tab sync preference yet (i.e. newly enabled type),
    // default to the session sync preference value.
    bool sessions_pref_value = pref_service->GetBoolean(prefs::kSyncSessions);
    pref_service->SetBoolean(prefs::kSyncTabs, sessions_pref_value);
  }
}

void ClearObsoleteUserTypePrefs(PrefService* pref_service) {
  for (const std::string& obsolete_pref : GetObsoleteUserTypePrefs()) {
    pref_service->ClearPref(obsolete_pref);
  }
}

void ClearObsoleteClearServerDataPrefs(PrefService* pref_service) {
  pref_service->ClearPref(kSyncPassphraseEncryptionTransitionInProgress);
  pref_service->ClearPref(kSyncNigoriStateForPassphraseTransition);
}

void ClearObsoleteAuthErrorPrefs(PrefService* pref_service) {
  pref_service->ClearPref(kSyncHasAuthError);
}

void ClearObsoleteFirstSyncTime(PrefService* pref_service) {
  pref_service->ClearPref(kSyncFirstSyncTime);
}

void ClearObsoleteSyncLongPollIntervalSeconds(PrefService* pref_service) {
  pref_service->ClearPref(kSyncLongPollIntervalSeconds);
}

#if defined(OS_CHROMEOS)
void ClearObsoleteSyncSpareBootstrapToken(PrefService* pref_service) {
  pref_service->ClearPref(kSyncSpareBootstrapToken);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace syncer
