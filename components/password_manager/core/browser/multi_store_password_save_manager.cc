// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
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

PendingCredentialsState ResolvePendingCredentialsStates(
    PendingCredentialsState profile_state,
    PendingCredentialsState account_state) {
  // Resolve the two states to a single canonical one, according to the
  // following hierarchy:
  // AUTOMATIC_SAVE > EQUAL_TO_SAVED_MATCH > UPDATE > NEW_LOGIN
  // Note that UPDATE and NEW_LOGIN will result in an Update or Save bubble to
  // be shown, while AUTOMATIC_SAVE and EQUAL_TO_SAVED_MATCH will result in a
  // silent save/update.
  // Some interesting cases:
  // NEW_LOGIN means that store doesn't know about the credential yet. If the
  // other store knows anything at all, then that always wins.
  // EQUAL_TO_SAVED_MATCH vs UPDATE: This means one store had a match, the other
  // had a mismatch (same username but different password). We want to silently
  // update the mismatch, which EQUAL achieves (since it'll still result in an
  // update to date_last_used, and updates always go to both stores).
  // TODO(crbug.com/1012203): AUTOMATIC_SAVE vs EQUAL: We should still perform
  // the silent update for EQUAL (so last_use_date gets updated).
  // TODO(crbug.com/1012203): AUTOMATIC_SAVE vs UPDATE: What's the expected
  // outcome? Currently we'll auto-save the PSL match and ignore the update
  // (which isn't too bad, since on the next submission the update will become
  // a silent update through EQUAL_TO_SAVED_MATCH).
  // TODO(crbug.com/1012203): AUTOMATIC_SAVE vs AUTOMATIC_SAVE: Somehow make
  // sure that the save goes to both stores.
  if (profile_state == PendingCredentialsState::AUTOMATIC_SAVE ||
      account_state == PendingCredentialsState::AUTOMATIC_SAVE) {
    return PendingCredentialsState::AUTOMATIC_SAVE;
  }
  if (profile_state == PendingCredentialsState::EQUAL_TO_SAVED_MATCH ||
      account_state == PendingCredentialsState::EQUAL_TO_SAVED_MATCH) {
    return PendingCredentialsState::EQUAL_TO_SAVED_MATCH;
  }
  if (profile_state == PendingCredentialsState::UPDATE ||
      account_state == PendingCredentialsState::UPDATE) {
    return PendingCredentialsState::UPDATE;
  }
  if (profile_state == PendingCredentialsState::NEW_LOGIN ||
      account_state == PendingCredentialsState::NEW_LOGIN) {
    return PendingCredentialsState::NEW_LOGIN;
  }
  NOTREACHED();
  return PendingCredentialsState::NONE;
}

}  // namespace

MultiStorePasswordSaveManager::MultiStorePasswordSaveManager(
    std::unique_ptr<FormSaver> profile_form_saver,
    std::unique_ptr<FormSaver> account_form_saver)
    : PasswordSaveManagerImpl(std::move(profile_form_saver)),
      account_store_form_saver_(std::move(account_form_saver)) {}

MultiStorePasswordSaveManager::~MultiStorePasswordSaveManager() = default;

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
      if (account_store_form_saver_ && IsOptedInForAccountStorage()) {
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
  if (account_store_form_saver_ && IsOptedInForAccountStorage()) {
    account_store_form_saver_->Update(
        pending_credentials_, AccountStoreMatches(matches), old_password);
  }
}

void MultiStorePasswordSaveManager::PermanentlyBlacklist(
    const PasswordStore::FormDigest& form_digest) {
  DCHECK(!client_->IsIncognito());
  if (account_store_form_saver_ && IsOptedInForAccountStorage() &&
      client_->GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
          PasswordForm::Store::kAccountStore) {
    account_store_form_saver_->PermanentlyBlacklist(form_digest);
  } else {
    // For users who aren't yet opted-in to the account storage, we store their
    // blacklisted entries in the profile store.
    form_saver_->PermanentlyBlacklist(form_digest);
  }
}

void MultiStorePasswordSaveManager::Unblacklist(
    const PasswordStore::FormDigest& form_digest) {
  // Try to unblacklist in both stores anyway because if credentials don't
  // exist, the unblacklist operation is no-op.
  form_saver_->Unblacklist(form_digest);
  if (account_store_form_saver_ && IsOptedInForAccountStorage()) {
    account_store_form_saver_->Unblacklist(form_digest);
  }
}

std::unique_ptr<PasswordSaveManager> MultiStorePasswordSaveManager::Clone() {
  auto result = std::make_unique<MultiStorePasswordSaveManager>(
      form_saver_->Clone(), account_store_form_saver_->Clone());
  CloneInto(result.get());
  return result;
}

void MultiStorePasswordSaveManager::MoveCredentialsToAccountStore() {
  // TODO(crbug.com/1032992): Moving credentials upon an update. FormFetch will
  // have an outdated credentials. Fix it if this turns out to be a product
  // requirement.

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

std::pair<const autofill::PasswordForm*, PendingCredentialsState>
MultiStorePasswordSaveManager::FindSimilarSavedFormAndComputeState(
    const PasswordForm& parsed_submitted_form) const {
  const std::vector<const PasswordForm*> matches =
      form_fetcher_->GetBestMatches();
  const PasswordForm* similar_saved_form_from_profile_store =
      password_manager_util::GetMatchForUpdating(parsed_submitted_form,
                                                 ProfileStoreMatches(matches));
  const PasswordForm* similar_saved_form_from_account_store =
      password_manager_util::GetMatchForUpdating(parsed_submitted_form,
                                                 AccountStoreMatches(matches));

  // Compute the PendingCredentialsState (i.e. what to do - save, update, silent
  // update) separately for the two stores.
  PendingCredentialsState profile_state = ComputePendingCredentialsState(
      parsed_submitted_form, similar_saved_form_from_profile_store);
  PendingCredentialsState account_state = ComputePendingCredentialsState(
      parsed_submitted_form, similar_saved_form_from_account_store);

  // Resolve the two states to a single canonical one.
  PendingCredentialsState state =
      ResolvePendingCredentialsStates(profile_state, account_state);

  // Choose which of the saved forms (if any) to use as the base for updating,
  // based on which of the two states won the resolution.
  // Note that if we got the same state for both stores, then it doesn't really
  // matter which one we pick for updating, since the result will be the same
  // anyway.
  const PasswordForm* similar_saved_form = nullptr;
  if (state == profile_state)
    similar_saved_form = similar_saved_form_from_profile_store;
  else if (state == account_state)
    similar_saved_form = similar_saved_form_from_account_store;

  return std::make_pair(similar_saved_form, state);
}

FormSaver* MultiStorePasswordSaveManager::GetFormSaverForGeneration() {
  return IsOptedInForAccountStorage() && account_store_form_saver_
             ? account_store_form_saver_.get()
             : form_saver_.get();
}

bool MultiStorePasswordSaveManager::IsOptedInForAccountStorage() {
  return client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage();
}

}  // namespace password_manager
