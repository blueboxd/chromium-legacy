// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
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
    const PasswordForm& pending,
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  switch (pending.in_store) {
    case PasswordForm::Store::kAccountStore:
      if (account_store_form_saver_ && IsAccountStoreActive())
        account_store_form_saver_->Save(pending, matches, old_password);
      break;
    case PasswordForm::Store::kProfileStore:
      form_saver_->Save(pending, matches, old_password);
      break;
    case PasswordForm::Store::kNotSet:
      if (account_store_form_saver_ && IsAccountStoreActive())
        account_store_form_saver_->Save(pending, matches, old_password);
      else
        form_saver_->Save(pending, matches, old_password);
      break;
  }
}

void MultiStorePasswordSaveManager::UpdateInternal(
    const PasswordForm& pending,
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  // Try to update both stores anyway because if credentials don't exist, the
  // update operation is no-op.
  form_saver_->Update(pending, matches, old_password);
  if (account_store_form_saver_ && IsAccountStoreActive()) {
    account_store_form_saver_->Update(pending, matches, old_password);
  }
}

bool MultiStorePasswordSaveManager::IsAccountStoreActive() {
  return client_->GetPasswordSyncState() ==
         password_manager::ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION;
}

}  // namespace password_manager
