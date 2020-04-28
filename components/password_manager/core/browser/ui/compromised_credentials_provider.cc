// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/containers/flat_set.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace password_manager {

namespace {

// Transparent comparator that can compare various types like CredentialView or
// CompromisedCredentials.
struct CredentialWithoutPassword {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return std::tie(lhs.signon_realm, lhs.username) <
           std::tie(rhs.signon_realm, rhs.username);
  }

  using is_transparent = void;
};

// This function takes two lists of compromised credentials and saved passwords
// and joins them, producing a new list that contains an entry for each element
// of |saved_passwords| whose signon_realm and username are also present in
// |compromised_credentials|.
std::vector<CredentialWithPassword>
JoinCompromisedCredentialsWithSavedPasswords(
    base::span<const CompromisedCredentials> compromised_credentials,
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  std::vector<CredentialWithPassword> compromised_credentials_with_passwords;
  compromised_credentials_with_passwords.reserve(
      compromised_credentials.size());

  // Since a single (signon_realm, username) pair might have multiple
  // corresponding entries in saved_passwords, we are using a multiset and doing
  // look-up via equal_range. In most cases the resulting |range| should have a
  // size of 1, however.
  std::multiset<CredentialView, CredentialWithoutPassword> credentials(
      saved_passwords.begin(), saved_passwords.end());
  for (const auto& compromised_credential : compromised_credentials) {
    auto range = credentials.equal_range(compromised_credential);
    // Make use of a set to only filter out repeated passwords, if any.
    base::flat_set<base::string16> passwords;
    std::for_each(range.first, range.second, [&](const CredentialView& view) {
      if (passwords.insert(view.password).second) {
        compromised_credentials_with_passwords.emplace_back(
            compromised_credential);
        compromised_credentials_with_passwords.back().password = view.password;
      }
    });
  }

  return compromised_credentials_with_passwords;
}

}  // namespace

bool operator==(const CredentialWithPassword& lhs,
                const CredentialWithPassword& rhs) {
  // Upcast `lhs` to trigger the CompromisedCredentials operator==.
  return static_cast<const CompromisedCredentials&>(lhs) == rhs &&
         lhs.password == rhs.password;
}

std::ostream& operator<<(std::ostream& out,
                         const CredentialWithPassword& credential) {
  return out << "{ signon_realm: " << credential.signon_realm
             << ", username: " << credential.username
             << ", create_time: " << credential.create_time
             << ", compromise_type: "
             << static_cast<int>(credential.compromise_type)
             << ", password: " << credential.password << " }";
}

CompromisedCredentialsProvider::CompromisedCredentialsProvider(
    scoped_refptr<PasswordStore> store,
    SavedPasswordsPresenter* presenter)
    : store_(std::move(store)), presenter_(presenter) {
  store_->AddDatabaseCompromisedCredentialsObserver(this);
  presenter_->AddObserver(this);
}

CompromisedCredentialsProvider::~CompromisedCredentialsProvider() {
  presenter_->RemoveObserver(this);
  store_->RemoveDatabaseCompromisedCredentialsObserver(this);
}

void CompromisedCredentialsProvider::Init() {
  store_->GetAllCompromisedCredentials(this);
}

CompromisedCredentialsProvider::CredentialsView
CompromisedCredentialsProvider::GetCompromisedCredentials() const {
  return compromised_credentials_with_passwords_;
}

void CompromisedCredentialsProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CompromisedCredentialsProvider::OnCompromisedCredentialsChanged() {
  // Cancel ongoing requests to the password store and issue a new request.
  cancelable_task_tracker()->TryCancelAll();
  store_->GetAllCompromisedCredentials(this);
}

// Re-computes the list of compromised credentials with passwords after
// obtaining a new list of compromised credentials.
void CompromisedCredentialsProvider::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  compromised_credentials_ = std::move(compromised_credentials);
  compromised_credentials_with_passwords_ =
      JoinCompromisedCredentialsWithSavedPasswords(
          compromised_credentials_, presenter_->GetSavedPasswords());
  NotifyCompromisedCredentialsChanged();
}

// Re-computes the list of compromised credentials with passwords after
// obtaining a new list of saved passwords.
void CompromisedCredentialsProvider::OnSavedPasswordsChanged(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  compromised_credentials_with_passwords_ =
      JoinCompromisedCredentialsWithSavedPasswords(compromised_credentials_,
                                                   saved_passwords);
  NotifyCompromisedCredentialsChanged();
}

void CompromisedCredentialsProvider::NotifyCompromisedCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.OnCompromisedCredentialsChanged(
        compromised_credentials_with_passwords_);
  }
}

}  // namespace password_manager
