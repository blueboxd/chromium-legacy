// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"

using autofill::PasswordForm;

namespace password_manager {

PasswordFeatureManagerImpl::PasswordFeatureManagerImpl(
    PrefService* pref_service,
    const syncer::SyncService* sync_service)
    : pref_service_(pref_service), sync_service_(sync_service) {}

bool PasswordFeatureManagerImpl::IsGenerationEnabled() const {
  switch (password_manager_util::GetPasswordSyncState(sync_service_)) {
    case NOT_SYNCING:
      return false;
    case SYNCING_WITH_CUSTOM_PASSPHRASE:
    case SYNCING_NORMAL_ENCRYPTION:
    case ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION:
      return true;
  }
}

bool PasswordFeatureManagerImpl::ShouldCheckReuseOnLeakDetection() const {
  switch (password_manager_util::GetPasswordSyncState(sync_service_)) {
    // We currently check the reuse of the leaked password only for users who
    // can access passwords.google.com. Therefore, if the credentials are not
    // synced, no need to check for password use.
    case NOT_SYNCING:
    case SYNCING_WITH_CUSTOM_PASSPHRASE:
      return false;
    case SYNCING_NORMAL_ENCRYPTION:
    case ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION:
      return true;
  }
}

bool PasswordFeatureManagerImpl::IsOptedInForAccountStorage() const {
  return password_manager_util::IsOptedInForAccountStorage(pref_service_,
                                                           sync_service_);
}

bool PasswordFeatureManagerImpl::ShouldShowAccountStorageOptIn() const {
  return password_manager_util::ShouldShowAccountStorageOptIn(pref_service_,
                                                              sync_service_);
}

void PasswordFeatureManagerImpl::SetAccountStorageOptIn(bool opt_in) {
  password_manager_util::SetAccountStorageOptIn(pref_service_, sync_service_,
                                                opt_in);
}

void PasswordFeatureManagerImpl::SetDefaultPasswordStore(
    const PasswordForm::Store& store) {
  DCHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kIsAccountStoreDefault,
                            store == PasswordForm::Store::kAccountStore);
}

PasswordForm::Store PasswordFeatureManagerImpl::GetDefaultPasswordStore()
    const {
  DCHECK(pref_service_);
  return pref_service_->GetBoolean(prefs::kIsAccountStoreDefault)
             ? PasswordForm::Store::kAccountStore
             : PasswordForm::Store::kProfileStore;
}

}  // namespace password_manager
