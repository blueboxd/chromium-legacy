// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_model_type_controller.h"

#include <utility>

#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

PasswordModelTypeController::PasswordModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    const base::RepeatingClosure& state_changed_callback)
    : ModelTypeController(syncer::PASSWORDS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      state_changed_callback_(state_changed_callback),
      account_storage_settings_watcher_(
          pref_service_,
          sync_service_,
          base::BindRepeating(
              &PasswordModelTypeController::OnOptInStateMaybeChanged,
              base::Unretained(this))) {
  identity_manager_->AddObserver(this);
}

PasswordModelTypeController::~PasswordModelTypeController() {
  identity_manager_->RemoveObserver(this);
}

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  sync_service_->AddObserver(this);
  sync_mode_ = configure_context.sync_mode;
  ModelTypeController::LoadModels(configure_context, model_load_callback);
  state_changed_callback_.Run();
}

void PasswordModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  sync_service_->RemoveObserver(this);
  // In transport-only mode, our storage is scoped to the Gaia account. That
  // means it should be cleared if Sync is stopped for any reason (other than
  // just browser shutdown). E.g. when switching to full-Sync mode, we don't
  // want to end up with two copies of the passwords (one in the profile DB, one
  // in the account DB).
  if (sync_mode_ == syncer::SyncMode::kTransportOnly) {
    switch (shutdown_reason) {
      case syncer::STOP_SYNC:
        shutdown_reason = syncer::DISABLE_SYNC;
        break;
      case syncer::DISABLE_SYNC:
      case syncer::BROWSER_SHUTDOWN:
        break;
    }
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
  state_changed_callback_.Run();
}

syncer::DataTypeController::PreconditionState
PasswordModelTypeController::GetPreconditionState() const {
  // If Sync-the-feature is enabled, then the user has opted in to that, and no
  // additional opt-in is required here.
  if (sync_service_->IsSyncFeatureEnabled())
    return PreconditionState::kPreconditionsMet;
  // If Sync-the-feature is *not* enabled, then password sync should only be
  // turned on if the user has opted in to the account-scoped storage.
  return features_util::IsOptedInForAccountStorage(pref_service_, sync_service_)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void PasswordModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(syncer::PASSWORDS);
  state_changed_callback_.Run();
}

void PasswordModelTypeController::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  // If the account information is stale, do nothing for now - wait until there
  // is fresh information.
  if (!accounts_in_cookie_jar_info.accounts_are_fresh) {
    return;
  }
  // Collect all the known accounts (signed-in or signed-out).
  std::vector<std::string> gaia_ids;
  for (const gaia::ListedAccount& account :
       accounts_in_cookie_jar_info.signed_in_accounts) {
    gaia_ids.push_back(account.gaia_id);
  }
  for (const gaia::ListedAccount& account :
       accounts_in_cookie_jar_info.signed_out_accounts) {
    gaia_ids.push_back(account.gaia_id);
  }
  // Keep any account-storage settings only for known accounts.
  features_util::KeepAccountStorageSettingsOnlyForUsers(pref_service_,
                                                        gaia_ids);
}

void PasswordModelTypeController::OnAccountsCookieDeletedByUserAction() {
  features_util::ClearAccountStorageSettingsForAllUsers(pref_service_);
}

void PasswordModelTypeController::OnOptInStateMaybeChanged() {
  // Note: This method gets called in many other situations as well, not just
  // when the opt-in state changes, but DataTypePreconditionChanged() is cheap
  // if nothing actually changed, so some spurious calls don't hurt.
  sync_service_->DataTypePreconditionChanged(syncer::PASSWORDS);
}

}  // namespace password_manager
