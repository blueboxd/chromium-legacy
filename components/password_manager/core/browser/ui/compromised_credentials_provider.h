// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/compromised_credentials_consumer.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "url/gurl.h"

namespace password_manager {

// Simple struct that augments the CompromisedCredentials with a password.
struct CredentialWithPassword : CompromisedCredentials {
  // Enable explicit construction from the parent struct. This will leave
  // |password| empty.
  explicit CredentialWithPassword(CompromisedCredentials credential)
      : CompromisedCredentials(std::move(credential)) {}

  base::string16 password;
};

bool operator==(const CredentialWithPassword& lhs,
                const CredentialWithPassword& rhs);

std::ostream& operator<<(std::ostream& out,
                         const CredentialWithPassword& credential);

// This class provides a read-only view over saved compromised credentials. It
// supports an observer interface, and clients can register themselves to get
// notified about changes to the list.
class CompromisedCredentialsProvider
    : public PasswordStore::DatabaseCompromisedCredentialsObserver,
      public CompromisedCredentialsConsumer,
      public SavedPasswordsPresenter::Observer {
 public:

  using CredentialsView = base::span<const CredentialWithPassword>;

  // Observer interface. Clients can implement this to get notified about
  // changes to the list of compromised credentials. Clients can register and
  // de-register themselves, and are expected to do so before the provider gets
  // out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCompromisedCredentialsChanged(
        CredentialsView credentials) = 0;
  };

  explicit CompromisedCredentialsProvider(scoped_refptr<PasswordStore> store,
                                          SavedPasswordsPresenter* presenter);
  ~CompromisedCredentialsProvider() override;

  void Init();

  // Returns a read-only view over the currently compromised credentials.
  CredentialsView GetCompromisedCredentials() const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // PasswordStore::DatabaseCompromisedCredentialsObserver:
  void OnCompromisedCredentialsChanged() override;

  // CompromisedCredentialsConsumer:
  void OnGetCompromisedCredentials(
      std::vector<CompromisedCredentials> compromised_credentials) override;

  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      SavedPasswordsPresenter::SavedPasswordsView passwords) override;

  // Notify observers about changes to
  // |compromised_credentials_with_passwords_|.
  void NotifyCompromisedCredentialsChanged();

  // The password store containing the compromised credentials.
  scoped_refptr<PasswordStore> store_;

  // A weak handle to the presenter used to join the list of compromised
  // credentials with saved passwords. Needs to outlive this instance.
  SavedPasswordsPresenter* presenter_ = nullptr;

  // Cache of the most recently obtained compromised credentials.
  std::vector<CompromisedCredentials> compromised_credentials_;

  // Cache of the most recently obtained compromised credentials with passwords.
  std::vector<CredentialWithPassword> compromised_credentials_with_passwords_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_
