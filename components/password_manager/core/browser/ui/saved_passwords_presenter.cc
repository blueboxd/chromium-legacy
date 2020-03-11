// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

SavedPasswordsPresenter::SavedPasswordsPresenter(
    scoped_refptr<PasswordStore> store)
    : store_(std::move(store)) {
  DCHECK(store_);
  store_->AddObserver(this);
}

SavedPasswordsPresenter::~SavedPasswordsPresenter() {
  store_->RemoveObserver(this);
}

void SavedPasswordsPresenter::Init() {
  store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

bool SavedPasswordsPresenter::EditPassword(
    const autofill::PasswordForm& password,
    base::string16 new_password) {
  auto found = std::find(passwords_.begin(), passwords_.end(), password);
  if (found == passwords_.end())
    return false;

  found->password_value = std::move(new_password);
  store_->UpdateLogin(*found);
  NotifyEdited(*found);
  return true;
}

SavedPasswordsPresenter::SavedPasswordsView
SavedPasswordsPresenter::GetSavedPasswords() const {
  return passwords_;
}

void SavedPasswordsPresenter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SavedPasswordsPresenter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SavedPasswordsPresenter::NotifyEdited(
    const autofill::PasswordForm& password) {
  for (auto& observer : observers_)
    observer.OnEdited(password);
}

void SavedPasswordsPresenter::NotifySavedPasswordsChanged() {
  for (auto& observer : observers_)
    observer.OnSavedPasswordsChanged(passwords_);
}

void SavedPasswordsPresenter::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  // Cancel ongoing requests to the password store and issue a new request.
  cancelable_task_tracker()->TryCancelAll();
  store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

void SavedPasswordsPresenter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  // Ignore blacklisted or federated credentials.
  base::EraseIf(results, [](const auto& form) {
    return form->blacklisted_by_user || form->IsFederatedCredential();
  });

  passwords_.resize(results.size());
  std::transform(results.begin(), results.end(), passwords_.begin(),
                 [](auto& result) { return std::move(*result); });
  NotifySavedPasswordsChanged();
}

}  // namespace password_manager
