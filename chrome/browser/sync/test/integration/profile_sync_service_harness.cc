// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"

#include <cstddef>
#include <sstream>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/engine/sync_string_conversions.h"

using syncer::ProfileSyncService;
using syncer::SyncCycleSnapshot;

namespace {

bool HasAuthError(ProfileSyncService* service) {
  return service->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

class EngineInitializeChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EngineInitializeChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    if (service()->IsEngineInitialized())
      return true;
    // Engine initialization is blocked by an auth error.
    if (HasAuthError(service()))
      return true;
    // Engine initialization is blocked by a failure to fetch Oauth2 tokens.
    if (service()->IsRetryingAccessTokenFetchForTest())
      return true;
    // Still waiting on engine initialization.
    return false;
  }

  std::string GetDebugMessage() const override { return "Engine Initialize"; }
};

class SyncSetupChecker : public SingleClientStatusChangeChecker {
 public:
  enum class State { kTransportActive, kFeatureActive };

  SyncSetupChecker(ProfileSyncService* service, State wait_for_state)
      : SingleClientStatusChangeChecker(service),
        wait_for_state_(wait_for_state) {}

  bool IsExitConditionSatisfied() override {
    syncer::SyncService::TransportState transport_state =
        service()->GetTransportState();
    if (transport_state == syncer::SyncService::TransportState::ACTIVE &&
        (wait_for_state_ != State::kFeatureActive ||
         service()->IsSyncFeatureActive())) {
      return true;
    }
    // Sync is blocked by an auth error.
    if (HasAuthError(service())) {
      return true;
    }
    if (service()->GetPassphraseRequiredReasonForTest() ==
        syncer::REASON_DECRYPTION) {
      LOG(FATAL)
          << "A passphrase is required for decryption but was not provided. "
             "Waiting for sync to become available won't succeed. Make sure "
             "to pass it when setting up sync.";
    }
    // Still waiting on sync setup.
    return false;
  }

  std::string GetDebugMessage() const override { return "Sync Setup"; }

 private:
  const State wait_for_state_;
};

}  // namespace

// static
std::unique_ptr<ProfileSyncServiceHarness> ProfileSyncServiceHarness::Create(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type) {
  return base::WrapUnique(
      new ProfileSyncServiceHarness(profile, username, password, signin_type));
}

ProfileSyncServiceHarness::ProfileSyncServiceHarness(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type)
    : profile_(profile),
      service_(ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
          profile)),
      username_(username),
      password_(password),
      signin_type_(signin_type),
      profile_debug_name_(profile->GetDebugName()) {}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() { }

bool ProfileSyncServiceHarness::SignInPrimaryAccount() {
  // TODO(crbug.com/871221): This function should distinguish primary account
  // (aka sync account) from secondary accounts (content area signin). Let's
  // migrate tests that exercise transport-only sync to secondary accounts.
  DCHECK(!username_.empty());

  switch (signin_type_) {
    case SigninType::UI_SIGNIN: {
      Browser* browser = chrome::FindBrowserWithProfile(profile_);
      DCHECK(browser);
      if (!login_ui_test_utils::SignInWithUI(browser, username_, password_)) {
        LOG(ERROR) << "Could not sign in to GAIA servers.";
        return false;
      }
      return true;
    }

    case SigninType::FAKE_SIGNIN: {
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);

      // Verify HasPrimaryAccount() separately because
      // MakePrimaryAccountAvailable() below DCHECK fails if there is already
      // an authenticated account.
      if (identity_manager->HasPrimaryAccount()) {
        DCHECK_EQ(identity_manager->GetPrimaryAccountInfo().email, username_);
        // Don't update the refresh token if we already have one. The reason is
        // that doing so causes Sync (ServerConnectionManager in particular) to
        // mark the current access token as invalid. Since tests typically
        // always hand out the same access token string, any new access token
        // acquired later would also be considered invalid.
        if (!identity_manager->HasPrimaryAccountWithRefreshToken()) {
          signin::SetRefreshTokenForPrimaryAccount(identity_manager);
        }
      } else {
        // Authenticate sync client using GAIA credentials.
        signin::MakePrimaryAccountAvailable(identity_manager, username_);
      }
      return true;
    }
  }

  NOTREACHED();
  return false;
}

#if !defined(OS_CHROMEOS)
void ProfileSyncServiceHarness::SignOutPrimaryAccount() {
  DCHECK(!username_.empty());
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_),
      signin::ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS);
}
#endif  // !OS_CHROMEOS

void ProfileSyncServiceHarness::EnterSyncPausedStateForPrimaryAccount() {
  DCHECK(service_->IsSyncFeatureActive());
  signin::SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_));
}

void ProfileSyncServiceHarness::ExitSyncPausedStateForPrimaryAccount() {
  signin::SetRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_));
}

bool ProfileSyncServiceHarness::SetupSync() {
  bool result =
      SetupSyncNoWaitForCompletion(syncer::UserSelectableTypeSet::All()) &&
      AwaitSyncSetupCompletion();
  if (!result) {
    LOG(ERROR) << profile_debug_name_ << ": SetupSync failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool ProfileSyncServiceHarness::SetupSyncNoWaitForCompletion(
    syncer::UserSelectableTypeSet selected_types) {
  return SetupSyncImpl(selected_types, EncryptionSetupMode::kNoEncryption,
                       /*encryption_passphrase=*/base::nullopt);
}

bool ProfileSyncServiceHarness::
    SetupSyncWithEncryptionPassphraseNoWaitForCompletion(
        syncer::UserSelectableTypeSet selected_types,
        const std::string& passphrase) {
  return SetupSyncImpl(selected_types, EncryptionSetupMode::kEncryption,
                       passphrase);
}

bool ProfileSyncServiceHarness::
    SetupSyncWithDecryptionPassphraseNoWaitForCompletion(
        syncer::UserSelectableTypeSet selected_types,
        const std::string& passphrase) {
  return SetupSyncImpl(selected_types, EncryptionSetupMode::kDecryption,
                       passphrase);
}

bool ProfileSyncServiceHarness::SetupSyncImpl(
    syncer::UserSelectableTypeSet selected_types,
    EncryptionSetupMode encryption_mode,
    const base::Optional<std::string>& passphrase) {
  DCHECK(encryption_mode == EncryptionSetupMode::kNoEncryption ||
         passphrase.has_value());
  DCHECK(!profile_->IsLegacySupervised())
      << "SetupSync should not be used for legacy supervised users.";

  if (service() == nullptr) {
    LOG(ERROR) << "SetupSync(): service() is null.";
    return false;
  }

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  sync_blocker_ = service()->GetSetupInProgressHandle();

  if (!SignInPrimaryAccount()) {
    return false;
  }

  // Now that auth is completed, request that sync actually start.
  service()->GetUserSettings()->SetSyncRequested(true);

  if (!AwaitEngineInitialization()) {
    return false;
  }
  // Choose the datatypes to be synced. If all datatypes are to be synced,
  // set sync_everything to true; otherwise, set it to false.
  bool sync_everything =
      (selected_types == syncer::UserSelectableTypeSet::All());
  service()->GetUserSettings()->SetSelectedTypes(sync_everything,
                                                 selected_types);

  if (encryption_mode == EncryptionSetupMode::kEncryption) {
    service()->GetUserSettings()->SetEncryptionPassphrase(passphrase.value());
  } else if (encryption_mode == EncryptionSetupMode::kDecryption) {
    if (!service()->GetUserSettings()->SetDecryptionPassphrase(
            passphrase.value())) {
      LOG(ERROR) << "WARNING: provided passphrase could not decrypt locally "
                    "present data.";
    }
  }
  // Notify ProfileSyncService that we are done with configuration.
  FinishSyncSetup();

  if ((signin_type_ == SigninType::UI_SIGNIN) &&
      !login_ui_test_utils::DismissSyncConfirmationDialog(
          chrome::FindBrowserWithProfile(profile_),
          base::TimeDelta::FromSeconds(30))) {
    LOG(ERROR) << "Failed to dismiss sync confirmation dialog.";
    return false;
  }

  // OneClickSigninSyncStarter observer is created with a real user sign in.
  // It is deleted on certain conditions which are not satisfied by our tests,
  // and this causes the SigninTracker observer to stay hanging at shutdown.
  // Calling LoginUIService::SyncConfirmationUIClosed forces the observer to
  // be removed. http://crbug.com/484388
  if (signin_type_ == SigninType::UI_SIGNIN) {
    LoginUIServiceFactory::GetForProfile(profile_)->SyncConfirmationUIClosed(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }

  return true;
}

void ProfileSyncServiceHarness::FinishSyncSetup() {
  sync_blocker_.reset();
  service()->GetUserSettings()->SetFirstSetupComplete();
}

void ProfileSyncServiceHarness::StopSyncServiceAndClearData() {
  DVLOG(1) << "Requesting stop for service and clearing data.";
  service()->StopAndClear();
}

void ProfileSyncServiceHarness::StopSyncServiceWithoutClearingData() {
  DVLOG(1) << "Requesting stop for service without clearing data.";
  service()->GetUserSettings()->SetSyncRequested(false);
}

bool ProfileSyncServiceHarness::StartSyncService() {
  std::unique_ptr<syncer::SyncSetupInProgressHandle> blocker =
      service()->GetSetupInProgressHandle();
  DVLOG(1) << "Requesting start for service";
  service()->GetUserSettings()->SetSyncRequested(true);

  if (!AwaitEngineInitialization()) {
    LOG(ERROR) << "AwaitEngineInitialization failed.";
    return false;
  }
  DVLOG(1) << "Engine Initialized successfully.";

  if (service()->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }
  DVLOG(1) << "Passphrase decryption success.";

  blocker.reset();
  service()->GetUserSettings()->SetFirstSetupComplete();

  if (!AwaitSyncSetupCompletion()) {
    LOG(FATAL) << "AwaitSyncSetupCompletion failed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::HasUnsyncedItems() {
  base::RunLoop loop;
  bool result = false;
  service()->HasUnsyncedItemsForTest(
      base::BindLambdaForTesting([&](bool has_unsynced_items) {
        result = has_unsynced_items;
        loop.Quit();
      }));
  loop.Run();
  return result;
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  std::vector<ProfileSyncServiceHarness*> harnesses;
  harnesses.push_back(this);
  harnesses.push_back(partner);
  return AwaitQuiescence(harnesses);
}

// static
bool ProfileSyncServiceHarness::AwaitQuiescence(
    const std::vector<ProfileSyncServiceHarness*>& clients) {
  if (clients.empty()) {
    return true;
  }

  std::vector<ProfileSyncService*> services;
  for (const ProfileSyncServiceHarness* harness : clients) {
    services.push_back(harness->service());
  }
  return QuiesceStatusChangeChecker(services).Wait();
}

bool ProfileSyncServiceHarness::AwaitEngineInitialization() {
  if (!EngineInitializeChecker(service()).Wait()) {
    LOG(ERROR) << "EngineInitializeChecker timed out.";
    return false;
  }

  if (!service()->IsEngineInitialized()) {
    LOG(ERROR) << "Service engine not initialized.";
    return false;
  }

  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::AwaitSyncSetupCompletion() {
  CHECK(service()->GetUserSettings()->IsFirstSetupComplete())
      << "Waiting for setup completion can only succeed after the first setup "
      << "got marked complete. Did you call SetupSync on this client?";
  if (!SyncSetupChecker(service(), SyncSetupChecker::State::kFeatureActive)
           .Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::AwaitSyncTransportActive() {
  if (!SyncSetupChecker(service(), SyncSetupChecker::State::kTransportActive)
           .Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::EnableSyncForType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "EnableSyncForType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (!IsSyncEnabledByUser()) {
    bool result =
        SetupSyncNoWaitForCompletion({type}) && AwaitSyncSetupCompletion();
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (selected_types.Has(type)) {
    DVLOG(1) << "EnableSyncForType(): Sync already enabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Put(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "EnableSyncForType(): Enabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForType failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "DisableSyncForType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (!selected_types.Has(type)) {
    DVLOG(1) << "DisableSyncForType(): Sync already disabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Remove(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "DisableSyncForType(): Disabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("DisableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::EnableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("EnableSyncForAllDatatypes");

  if (!IsSyncEnabledByUser()) {
    bool result = SetupSync();
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->GetUserSettings()->SetSelectedTypes(
      true, syncer::UserSelectableTypeSet::All());

  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes "
             << "on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForAllDatatypes failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("DisableSyncForAllDatatypes");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->StopAndClear();

  DVLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all "
           << "datatypes on " << profile_debug_name_;
  return true;
}

SyncCycleSnapshot ProfileSyncServiceHarness::GetLastCycleSnapshot() const {
  DCHECK(service() != nullptr) << "Sync service has not yet been set up.";
  if (service()->IsSyncFeatureActive()) {
    return service()->GetLastCycleSnapshotForDebugging();
  }
  return SyncCycleSnapshot();
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  std::unique_ptr<base::DictionaryValue> value(
      syncer::sync_ui_util::ConstructAboutInformation(service(),
                                                      chrome::GetChannel()));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(
      *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &service_status);
  return service_status;
}

// TODO(sync): Clean up this method in a separate CL. Remove all snapshot fields
// and log shorter, more meaningful messages.
std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) const {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncCycleSnapshot& snap = GetLastCycleSnapshot();
    syncer::SyncStatus status;
    service()->QueryDetailedSyncStatusForDebugging(&status);
    // Capture select info from the sync session snapshot and syncer status.
    os << ", has_unsynced_items: " << snap.has_remaining_local_changes()
       << ", did_commit: "
       << (snap.model_neutral_state().num_successful_commits == 0 &&
           snap.model_neutral_state().commit_result.value() ==
               syncer::SyncerError::SYNCER_OK)
       << ", encryption conflicts: " << snap.num_encryption_conflicts()
       << ", hierarchy conflicts: " << snap.num_hierarchy_conflicts()
       << ", server conflicts: " << snap.num_server_conflicts()
       << ", num_updates_downloaded : "
       << snap.model_neutral_state().num_updates_downloaded_total
       << ", passphrase_required_reason: "
       << syncer::PassphraseRequiredReasonToString(
              service()->GetPassphraseRequiredReasonForTest())
       << ", notifications_enabled: " << status.notifications_enabled
       << ", service_is_active: " << service()->IsSyncFeatureActive();
  } else {
    os << "Sync service not available";
  }
  return os.str();
}

bool ProfileSyncServiceHarness::IsSyncEnabledByUser() const {
  return service()->GetUserSettings()->IsFirstSetupComplete() &&
         !service()->HasDisableReason(
             ProfileSyncService::DISABLE_REASON_USER_CHOICE);
}
