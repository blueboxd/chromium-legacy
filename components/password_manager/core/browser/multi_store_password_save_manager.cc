// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::PasswordForm;

namespace password_manager {

MultiStorePasswordSaveManager::MultiStorePasswordSaveManager(
    std::unique_ptr<FormSaver> profile_form_saver,
    std::unique_ptr<FormSaver> account_form_saver)
    : PasswordSaveManagerImpl(std::move(profile_form_saver)),
      account_store_form_saver_(std::move(account_form_saver)) {}

MultiStorePasswordSaveManager::~MultiStorePasswordSaveManager() = default;

FormSaver* MultiStorePasswordSaveManager::GetFormSaverForGeneration() {
  return IsAccountStoreActive() && account_store_form_saver_
             ? account_store_form_saver_.get()
             : form_saver_.get();
}

void MultiStorePasswordSaveManager::SaveInternal(
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  // For New Credentials, we should respect the default password store selected
  // by user. In other cases such PSL matching, we respect the store in the
  // retrieved credentials.
  if (pending_credentials_state_ == PendingCredentialsState::NEW_LOGIN) {
    pending_credentials_.in_store =
        client_->GetPasswordFeatureManager()->GetDefaultPasswordStore();
  }

  switch (pending_credentials_.in_store) {
    case PasswordForm::Store::kAccountStore:
      if (account_store_form_saver_ && IsAccountStoreActive())
        account_store_form_saver_->Save(pending_credentials_, matches,
                                        old_password);
      break;
    case PasswordForm::Store::kProfileStore:
      form_saver_->Save(pending_credentials_, matches, old_password);
      break;
    case PasswordForm::Store::kNotSet:
      if (account_store_form_saver_ && IsAccountStoreActive())
        account_store_form_saver_->Save(pending_credentials_, matches,
                                        old_password);
      else
        form_saver_->Save(pending_credentials_, matches, old_password);
      break;
  }
}

void MultiStorePasswordSaveManager::UpdateInternal(
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  // Try to update both stores anyway because if credentials don't exist, the
  // update operation is no-op.
  form_saver_->Update(pending_credentials_, matches, old_password);
  if (account_store_form_saver_ && IsAccountStoreActive()) {
    account_store_form_saver_->Update(pending_credentials_, matches,
                                      old_password);
  }
}

void MultiStorePasswordSaveManager::PermanentlyBlacklist(
    const PasswordStore::FormDigest& form_digest) {
  DCHECK(!client_->IsIncognito());
  if (account_store_form_saver_ && IsAccountStoreActive() &&
      client_->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
          PasswordForm::Store::kAccountStore) {
    account_store_form_saver_->PermanentlyBlacklist(form_digest);
  } else {
    form_saver_->PermanentlyBlacklist(form_digest);
  }
}

void MultiStorePasswordSaveManager::Unblacklist(
    const PasswordStore::FormDigest& form_digest) {
  // Try to unblacklist in both stores anyway because if credentials don't
  // exist, the unblacklist operation is no-op.
  form_saver_->Unblacklist(form_digest);
  if (account_store_form_saver_ && IsAccountStoreActive()) {
    account_store_form_saver_->Unblacklist(form_digest);
  }
}

bool MultiStorePasswordSaveManager::IsAccountStoreActive() {
  return client_->GetPasswordSyncState() ==
         password_manager::ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION;
}

}  // namespace password_manager
