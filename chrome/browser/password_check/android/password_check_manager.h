// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/password_check/android/password_check_ui_status.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service_interface.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PasswordCheckManager
    : public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::CompromisedCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnSavedPasswordsFetched(int count) = 0;
    virtual void OnCompromisedCredentialsChanged(int count) = 0;
    virtual void OnPasswordCheckStatusChanged(
        password_manager::PasswordCheckUIStatus status) = 0;
  };

  struct CompromisedCredentialForUI : password_manager::CredentialWithPassword {
    explicit CompromisedCredentialForUI(
        const password_manager::CredentialWithPassword& credential);

    CompromisedCredentialForUI(const CompromisedCredentialForUI& other);
    CompromisedCredentialForUI(CompromisedCredentialForUI&& other);
    CompromisedCredentialForUI& operator=(
        const CompromisedCredentialForUI& other);
    CompromisedCredentialForUI& operator=(CompromisedCredentialForUI&& other);
    ~CompromisedCredentialForUI();

    base::string16 display_username;
    base::string16 display_origin;
    std::string package_name;
    std::string change_password_url;
    bool is_android_credential = false;
    bool has_script = false;
  };

  // `observer` must outlive `this`.
  PasswordCheckManager(Profile* profile, Observer* observer);
  ~PasswordCheckManager() override;

  // Requests to start the password check.
  void StartCheck();

  // Stops a running check.
  void StopCheck();

  // Called by java to retrieve the number of compromised credentials. If the
  // credentials haven't been fetched yet, this will return 0.
  int GetCompromisedCredentialsCount() const;

  // Called by java to retrieve the number of saved passwords.
  // If the saved passwords haven't been fetched yet, this will return 0.
  int GetSavedPasswordsCount() const;

  // Called by java to retrieve the compromised credentials.
  std::vector<CompromisedCredentialForUI> GetCompromisedCredentials() const;

  // Not copyable or movable
  PasswordCheckManager(const PasswordCheckManager&) = delete;
  PasswordCheckManager& operator=(const PasswordCheckManager&) = delete;
  PasswordCheckManager(PasswordCheckManager&&) = delete;
  PasswordCheckManager& operator=(PasswordCheckManager&&) = delete;

 private:
  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords)
      override;

  // CompromisedCredentialsManager::Observer
  void OnCompromisedCredentialsChanged(
      password_manager::CompromisedCredentialsManager::CredentialsView
          credentials) override;

  // BulkLeakCheckServiceInterface::Observer
  void OnStateChanged(
      password_manager::BulkLeakCheckServiceInterface::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;
  void OnBulkCheckServiceShutDown() override;

  // Turns a `CredentialWithPassword` into a `CompromisedCredentialForUI`,
  // getting suitable strings for all display elements (e.g. url, app name,
  // app package, username, etc.).
  CompromisedCredentialForUI MakeUICredential(
      const password_manager::CredentialWithPassword& credential) const;

  // Converts the state retrieved from the check service into a state that
  // can be used by the UI to display appropriate messages.
  password_manager::PasswordCheckUIStatus GetUIStatus(
      password_manager::BulkLeakCheckServiceInterface::State state) const;

  // Returns true if the user has their passwords available in their Google
  // Account. Used to determine whether the user could use the password check
  // in the account if the quota limit was reached.
  bool CanUseAccountCheck() const;

  // Obsever being notified of UI-relevant events.
  // It must outlive `this`.
  Observer* observer_ = nullptr;

  // The profile for which the passwords are checked.
  Profile* profile_ = nullptr;

  // Handle to the password store, powering both `saved_passwords_presenter_`
  // and `compromised_credentials_manager_`.
  scoped_refptr<password_manager::PasswordStore> password_store_ =
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::EXPLICIT_ACCESS);

  // Used by `compromised_credentials_manager_` to obtain the list of saved
  // passwords.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_{
      password_store_};

  // Used to obtain the list of compromised credentials.
  password_manager::CompromisedCredentialsManager
      compromised_credentials_manager_{password_store_,
                                       &saved_passwords_presenter_};

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_{
          &saved_passwords_presenter_,
          BulkLeakCheckServiceFactory::GetForProfile(profile_),
          profile_->GetPrefs()};

  // This is true when the saved passwords have been fetched from the store.
  bool is_initialized_ = false;

  // Whether the check start was requested.
  bool was_start_requested_ = false;

  // A scoped observer for `saved_passwords_presenter_`.
  ScopedObserver<password_manager::SavedPasswordsPresenter,
                 password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for `compromised_credentials_manager_`.
  ScopedObserver<password_manager::CompromisedCredentialsManager,
                 password_manager::CompromisedCredentialsManager::Observer>
      observed_compromised_credentials_manager_{this};

  // A scoped observer for the BulkLeakCheckService.
  ScopedObserver<password_manager::BulkLeakCheckServiceInterface,
                 password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_bulk_leak_check_service_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_MANAGER_H_
