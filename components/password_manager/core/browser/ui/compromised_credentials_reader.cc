// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_reader.h"

#include <iterator>

#include "base/stl_util.h"
#include "base/util/ranges/algorithm.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {
CompromisedCredentialsReader::CompromisedCredentialsReader(
    PasswordStore* profile_store,
    PasswordStore* account_store)
    : profile_store_(profile_store), account_store_(account_store) {
  DCHECK(profile_store_);
  observed_password_store_.Add(profile_store_);
  if (account_store_)
    observed_password_store_.Add(account_store_);
}

CompromisedCredentialsReader::~CompromisedCredentialsReader() = default;

void CompromisedCredentialsReader::Init() {
  profile_store_->GetAllCompromisedCredentials(this);
  if (account_store_)
    account_store_->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsReader::OnCompromisedCredentialsChanged() {
  // This class overrides OnCompromisedCredentialsChangedIn() (the version of
  // this method that also receives the originating store), so the store-less
  // version never gets called.
  NOTREACHED();
}

void CompromisedCredentialsReader::OnCompromisedCredentialsChangedIn(
    PasswordStore* store) {
  store->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsReader::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  // This class overrides OnGetCompromisedCredentialFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void CompromisedCredentialsReader::OnGetCompromisedCredentialsFrom(
    PasswordStore* store,
    std::vector<CompromisedCredentials> compromised_credentials) {
  // Remove all previously cached credentials from `store` and then insert
  // the just received `compromised_credentials`.
  autofill::PasswordForm::Store to_remove =
      store == profile_store_ ? autofill::PasswordForm::Store::kProfileStore
                              : autofill::PasswordForm::Store::kAccountStore;

  base::EraseIf(compromised_credentials_, [to_remove](const auto& credential) {
    return credential.in_store == to_remove;
  });

  util::ranges::move(compromised_credentials,
                     std::back_inserter(compromised_credentials_));

  // Inform the observers
  for (auto& observer : observers_)
    observer.OnCompromisedCredentialsChanged(compromised_credentials_);
}

void CompromisedCredentialsReader::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsReader::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace password_manager
