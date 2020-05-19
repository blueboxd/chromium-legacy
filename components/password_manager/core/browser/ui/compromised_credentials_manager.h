// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_MANAGER_H_

#include <map>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observer.h"
#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/compromised_credentials_consumer.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "url/gurl.h"

namespace password_manager {

class LeakCheckCredential;

enum class CompromiseTypeFlags {
  kNotCompromised = 0,
  // If the credentials was leaked by a data breach.
  kCredentialLeaked = 1 << 0,
  // If the credentials was reused on a phishing site.
  kCredentialPhished = 1 << 1,
};

constexpr CompromiseTypeFlags operator|(CompromiseTypeFlags lhs,
                                        CompromiseTypeFlags rhs) {
  return static_cast<CompromiseTypeFlags>(static_cast<int>(lhs) |
                                          static_cast<int>(rhs));
}

// Simple struct that augments key values of CompromisedCredentials and a
// password.
struct CredentialView {
  CredentialView() = default;
  // Enable explicit construction from the autofill::PasswordForm
  explicit CredentialView(const autofill::PasswordForm& form)
      : signon_realm(form.signon_realm),
        username(form.username_value),
        password(form.password_value) {}

  std::string signon_realm;
  base::string16 username;
  base::string16 password;
};

// All information needed by UI to represent CompromisedCredential. It's a
// result of deduplicating CompromisedCredentials to have single entity both for
// phished and leaked credentials with latest |create_time|, and after that
// joining with autofill::PasswordForms to get passwords.
struct CredentialWithPassword : CredentialView {
  explicit CredentialWithPassword(const CredentialView& credential);
  explicit CredentialWithPassword(const CompromisedCredentials& credential);

  CredentialWithPassword(const CredentialWithPassword& other);
  CredentialWithPassword(CredentialWithPassword&& other);
  ~CredentialWithPassword();

  CredentialWithPassword& operator=(const CredentialWithPassword& other);
  CredentialWithPassword& operator=(CredentialWithPassword&& other);
  base::Time create_time;
  CompromiseTypeFlags compromise_type = CompromiseTypeFlags::kNotCompromised;
};

// Comparator that can compare CredentialView or CredentialsWithPasswords.
struct PasswordCredentialLess {
  bool operator()(const CredentialView& lhs, const CredentialView& rhs) const {
    return std::tie(lhs.signon_realm, lhs.username, lhs.password) <
           std::tie(rhs.signon_realm, rhs.username, rhs.password);
  }
};

// Extra information about CompromisedCredentials which is required by UI.
struct CredentialMetadata;

// This class provides clients with saved compromised credentials and
// possibility to save new LeakedCredentials, edit/delete compromised
// credentials and match compromised credentials with corresponding
// autofill::PasswordForms. It supports an observer interface, and clients can
// register themselves to get notified about changes to the list.
class CompromisedCredentialsManager
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

  explicit CompromisedCredentialsManager(scoped_refptr<PasswordStore> store,
                                         SavedPasswordsPresenter* presenter);
  ~CompromisedCredentialsManager() override;

  void Init();

  // Marks all saved credentials which have same username & password as
  // compromised.
  void SaveCompromisedCredential(const LeakCheckCredential& credential);

  // Attempts to change the stored password of |credential| to |new_password|.
  // Returns whether the change succeeded.
  bool UpdateCompromisedCredentials(const CredentialView& credential,
                                    const base::StringPiece password);

  // Attempts to remove |credential| from the password store. Returns whether
  // the remove succeeded.
  bool RemoveCompromisedCredential(const CredentialView& credential);

  // Returns a vector of currently compromised credentials.
  std::vector<CredentialWithPassword> GetCompromisedCredentials() const;

  // Returns password forms which map to provided compromised credential.
  // In most of the cases vector will have 1 element only.
  SavedPasswordsPresenter::SavedPasswordsView GetSavedPasswordsFor(
      const CredentialView& credential) const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  using CredentialPasswordsMap =
      std::map<CredentialView, CredentialMetadata, PasswordCredentialLess>;

  // PasswordStore::DatabaseCompromisedCredentialsObserver:
  void OnCompromisedCredentialsChanged() override;

  // CompromisedCredentialsConsumer:
  void OnGetCompromisedCredentials(
      std::vector<CompromisedCredentials> compromised_credentials) override;

  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      SavedPasswordsPresenter::SavedPasswordsView passwords) override;

  // Function to update |credentials_to_forms_| and notify observers.
  void UpdateCachedDataAndNotifyObservers(
      SavedPasswordsPresenter::SavedPasswordsView saved_passwords);

  // The password store containing the compromised credentials.
  scoped_refptr<PasswordStore> store_;

  // A weak handle to the presenter used to join the list of compromised
  // credentials with saved passwords. Needs to outlive this instance.
  SavedPasswordsPresenter* presenter_ = nullptr;

  // Cache of the most recently obtained compromised credentials.
  std::vector<CompromisedCredentials> compromised_credentials_;

  // A map that matches CredentialView to corresponding PasswordForms, latest
  // create_type and combined compromise type.
  CredentialPasswordsMap credentials_to_forms_;

  // A scoped observer for |store_| to listen changes related to
  // CompromisedCredentials only.
  ScopedObserver<PasswordStore,
                 PasswordStore::DatabaseCompromisedCredentialsObserver,
                 &PasswordStore::AddDatabaseCompromisedCredentialsObserver,
                 &PasswordStore::RemoveDatabaseCompromisedCredentialsObserver>
      observed_password_store_{this};

  // A scoped observer for |presenter_|.
  ScopedObserver<SavedPasswordsPresenter, SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_MANAGER_H_
