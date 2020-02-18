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
#include "url/gurl.h"

namespace password_manager {

// This class provides a read-only view over saved compromised credentials. It
// supports an observer interface, and clients can register themselves to get
// notified about changes to the list.
class CompromisedCredentialsProvider
    : public PasswordStore::DatabaseCompromisedCredentialsObserver,
      public CompromisedCredentialsConsumer {
 public:
  // Simple struct that augments the CompromisedCredentials with a password.
  struct CredentialWithPassword : CompromisedCredentials {
    // Enable explicit construction and assignment from the parent struct. These
    // will leave |password| empty.
    explicit CredentialWithPassword(CompromisedCredentials credential)
        : CompromisedCredentials(std::move(credential)) {}

    CredentialWithPassword& operator=(CompromisedCredentials credential) {
      CompromisedCredentials::operator=(std::move(credential));
      password.clear();
      return *this;
    }

    base::string16 password;
  };

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

  explicit CompromisedCredentialsProvider(scoped_refptr<PasswordStore> store);
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

  // Notify observers about changes to |compromised_credentials_|.
  void NotifyCompromisedCredentialsChanged();

  // The password store containing the compromised credentials.
  scoped_refptr<PasswordStore> store_;

  // Cache of the most recently obtained compromised credentials.
  std::vector<CredentialWithPassword> compromised_credentials_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_
