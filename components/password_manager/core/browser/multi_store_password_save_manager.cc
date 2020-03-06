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

bool AccountStoreMatchesContainForm(
    const std::vector<const PasswordForm*>& matches,
    const PasswordForm& form) {
  PasswordForm form_in_account_store(form);
  form_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  for (const PasswordForm* match : matches) {
    if (form_in_account_store == *match)
      return true;
  }
  return false;
}

}  // namespace

MultiStorePasswordSaveManager::MultiStorePasswordSaveManager(
    std::unique_ptr<FormSaver> profile_form_saver,
    std::unique_ptr<FormSaver> account_form_saver)
    : PasswordSaveManagerImpl(std::move(profile_form_saver)),
      account_store_form_saver_(std::move(account_form_saver)) {}

MultiStorePasswordSaveManager::~MultiStorePasswordSaveManager() = default;

FormSaver* MultiStorePasswordSaveManager::GetFormSaverForGeneration() {
  return IsAccountStoreEnabled() && account_store_form_saver_
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
      if (account_store_form_saver_ && IsAccountStoreEnabled()) {
        account_store_form_saver_->Save(
            pending_credentials_, AccountStoreMatches(matches), old_password);
      }
      // TODO(crbug.com/1012203): Record UMA for how many passwords get dropped
      // here. In rare cases it could happen that the user *was* opted in when
      // the save dialog was shown, but now isn't anymore.
      break;
    case PasswordForm::Store::kProfileStore:
      form_saver_->Save(pending_credentials_, ProfileStoreMatches(matches),
                        old_password);
      break;
    case PasswordForm::Store::kNotSet:
      NOTREACHED();
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
  if (account_store_form_saver_ && IsAccountStoreEnabled()) {
    account_store_form_saver_->Update(
        pending_credentials_, AccountStoreMatches(matches), old_password);
  }
}

void MultiStorePasswordSaveManager::PermanentlyBlacklist(
    const PasswordStore::FormDigest& form_digest) {
  DCHECK(!client_->IsIncognito());
  if (account_store_form_saver_ && IsAccountStoreEnabled() &&
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
  if (account_store_form_saver_ && IsAccountStoreEnabled()) {
    account_store_form_saver_->Unblacklist(form_digest);
  }
}

std::unique_ptr<PasswordSaveManager> MultiStorePasswordSaveManager::Clone() {
  auto result = std::make_unique<MultiStorePasswordSaveManager>(
      form_saver_->Clone(), account_store_form_saver_->Clone());
  CloneInto(result.get());
  return result;
}

bool MultiStorePasswordSaveManager::IsAccountStoreEnabled() {
  return client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage();
}

void MultiStorePasswordSaveManager::MoveCredentialsToAccountStore() {
  // TODO(crbug.com/1032992): There are other rare corner cases that should
  // still be handled:
  // 1. Credential exists only in the profile store but a PSL matched one exists
  // in both profile and account store.
  // 2. Moving credentials upon an update. FormFetch will have an outdated
  // credentials. Fix it if this turns out to be a product requirement.

  std::vector<const PasswordForm*> account_store_matches =
      AccountStoreMatches(form_fetcher_->GetNonFederatedMatches());
  const std::vector<const PasswordForm*> account_store_federated_matches =
      AccountStoreMatches(form_fetcher_->GetFederatedMatches());
  account_store_matches.insert(account_store_matches.end(),
                               account_store_federated_matches.begin(),
                               account_store_federated_matches.end());

  std::vector<const PasswordForm*> profile_store_matches =
      ProfileStoreMatches(form_fetcher_->GetNonFederatedMatches());
  const std::vector<const PasswordForm*> profile_store_federated_matches =
      ProfileStoreMatches(form_fetcher_->GetFederatedMatches());
  profile_store_matches.insert(profile_store_matches.end(),
                               profile_store_federated_matches.begin(),
                               profile_store_federated_matches.end());

  for (const PasswordForm* match : profile_store_matches) {
    DCHECK(!match->IsUsingAccountStore());
    // Ignore credentials matches for other usernames.
    if (match->username_value != pending_credentials_.username_value)
      continue;

    // Don't call Save() if the credential already exists in the account
    // store, 1) to avoid unnecessary sync cycles, 2) to avoid potential
    // last_used_date update.
    if (!AccountStoreMatchesContainForm(account_store_matches, *match)) {
      account_store_form_saver_->Save(*match, account_store_matches,
                                      /*old_password=*/base::string16());
    }
    form_saver_->Remove(*match);
  }
}

}  // namespace password_manager
