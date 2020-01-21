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

namespace {

std::vector<const PasswordForm*> MatchesInStore(
    const std::vector<const PasswordForm*>& matches,
    PasswordForm::Store store) {
  std::vector<const PasswordForm*> store_matches;
  for (const PasswordForm* match : matches) {
    DCHECK(match->in_store != PasswordForm::Store::kNotSet);
    if (match->in_store == store)
      store_matches.push_back(match);
  }
  return store_matches;
}

std::vector<const PasswordForm*> AccountStoreMatches(
    const std::vector<const PasswordForm*>& matches) {
  return MatchesInStore(matches, PasswordForm::Store::kAccountStore);
}

std::vector<const PasswordForm*> ProfileStoreMatches(
    const std::vector<const PasswordForm*>& matches) {
  return MatchesInStore(matches, PasswordForm::Store::kProfileStore);
}

}  // namespace

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
        account_store_form_saver_->Save(
            pending_credentials_, AccountStoreMatches(matches), old_password);
      break;
    case PasswordForm::Store::kProfileStore:
      form_saver_->Save(pending_credentials_, ProfileStoreMatches(matches),
                        old_password);
      break;
    case PasswordForm::Store::kNotSet:
      if (account_store_form_saver_ && IsAccountStoreActive())
        account_store_form_saver_->Save(
            pending_credentials_, AccountStoreMatches(matches), old_password);
      else
        form_saver_->Save(pending_credentials_, ProfileStoreMatches(matches),
                          old_password);
      break;
  }
}

void MultiStorePasswordSaveManager::UpdateInternal(
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  // Try to update both stores anyway because if credentials don't exist, the
  // update operation is no-op.
  form_saver_->Update(pending_credentials_, ProfileStoreMatches(matches),
                      old_password);
  if (account_store_form_saver_ && IsAccountStoreActive()) {
    account_store_form_saver_->Update(
        pending_credentials_, AccountStoreMatches(matches), old_password);
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

std::unique_ptr<PasswordSaveManager> MultiStorePasswordSaveManager::Clone() {
  auto result = std::make_unique<MultiStorePasswordSaveManager>(
      form_saver_->Clone(), account_store_form_saver_->Clone());
  CloneInto(result.get());
  return result;
}

bool MultiStorePasswordSaveManager::IsAccountStoreActive() {
  return client_->GetPasswordSyncState() ==
         password_manager::ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION;
}

void MultiStorePasswordSaveManager::MoveCredentialsToAccountStore() {
  // TODO(crbug.com/1032992): There are other rare corner cases that should
  // still be handled: 0. Moving PSL matched credentials doesn't work now
  // because of
  // https://cs.chromium.org/chromium/src/components/password_manager/core/browser/login_database.cc?l=1318&rcl=e32055d4843e9fc1fa920c5f1f83c1313607e28a
  // 1. Credential exists only in the profile store but with an outdated
  // password.
  // 2. Credentials exist in both stores.
  // 3. Credentials exist in both stores while one of them of outdated. (profile
  // or remote).
  // 4. Credential exists only in the profile store but a PSL matched one exists
  // in both profile and account store.

  const std::vector<const PasswordForm*> account_store_matches =
      AccountStoreMatches(form_fetcher_->GetBestMatches());
  for (const PasswordForm* match :
       ProfileStoreMatches(form_fetcher_->GetBestMatches())) {
    DCHECK(!match->IsUsingAccountStore());
    // Ignore credentials matches for other usernames.
    if (match->username_value != pending_credentials_.username_value)
      continue;

    account_store_form_saver_->Save(*match, account_store_matches,
                                    /*old_password=*/base::string16());
    form_saver_->Remove(*match);
  }
}

}  // namespace password_manager
