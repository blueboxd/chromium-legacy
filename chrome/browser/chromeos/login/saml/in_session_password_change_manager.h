// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "chromeos/login/auth/auth_status_consumer.h"

class Profile;

namespace user_manager {
class User;
}

namespace chromeos {
class CryptohomeAuthenticator;
class UserContext;

// Manages the flow of changing a password in-session - handles user
// response from dialogs, and callbacks from subsystems.
class InSessionPasswordChangeManager : public AuthStatusConsumer {
 public:
  // Events in the in-session SAML password change flow.
  enum Event {
    CHANGE_PASSWORD_AUTH_FAILURE,
    // TODO(https://crbug.com/930109): Add more useful events.
  };

  // Observers of InSessionPasswordChangeManager are notified of certain events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEvent(Event event) = 0;
  };

  // Returns null if in-session password change is disabled.
  static std::unique_ptr<InSessionPasswordChangeManager> CreateIfEnabled(
      Profile* primary_profile);

  explicit InSessionPasswordChangeManager(Profile* primary_profile);
  ~InSessionPasswordChangeManager() override;

  // Start the in-session password change flow by showing a dialog that embeds
  // the user's SAML IdP change-password page:
  void StartInSessionPasswordChange();

  // Handle a SAML password change. |old_password| and |new_password| can be
  // empty if scraping failed, in which case the user will be prompted to enter
  // them again. If they are scraped, this calls ChangePassword immediately,
  void OnSamlPasswordChanged(const std::string& scraped_old_password,
                             const std::string& scraped_new_password);

  // Change cryptohome password for primary user.
  void ChangePassword(const std::string& old_password,
                      const std::string& new_password);

  // Handle a failure to scrape the passwords during in-session password change,
  // by showing a dialog for the user to confirm their old + new password.
  void HandlePasswordScrapeFailure();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

 private:
  void NotifyObservers(Event event);

  Profile* primary_profile_;
  const user_manager::User* primary_user_;
  base::ObserverList<Observer> observer_list_;

  scoped_refptr<CryptohomeAuthenticator> authenticator_;

  DISALLOW_COPY_AND_ASSIGN(InSessionPasswordChangeManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_
