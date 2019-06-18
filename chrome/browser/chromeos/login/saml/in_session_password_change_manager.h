// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
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
  // Returns null if in-session password change is disabled.
  static std::unique_ptr<InSessionPasswordChangeManager> CreateIfEnabled(
      Profile* primary_profile,
      const user_manager::User* primary_user);

  InSessionPasswordChangeManager(Profile* primary_profile,
                                 const user_manager::User* primary_user);
  ~InSessionPasswordChangeManager() override;

  // Change cryptohome password for primary user.
  void ChangePassword(const std::string& old_password,
                      const std::string& new_password);

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

 private:
  Profile* primary_profile_;
  const user_manager::User* primary_user_;

  scoped_refptr<CryptohomeAuthenticator> authenticator_;

  DISALLOW_COPY_AND_ASSIGN(InSessionPasswordChangeManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_IN_SESSION_PASSWORD_CHANGE_MANAGER_H_
