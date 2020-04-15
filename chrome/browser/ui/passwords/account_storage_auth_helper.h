// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_
#define CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_

#include "base/callback.h"
#include "components/password_manager/core/browser/password_manager_client.h"

struct CoreAccountId;
class Profile;

// Responsible for triggering authentication flows related to the passwords
// account storage. Used only by desktop.
class AccountStorageAuthHelper {
 public:
  explicit AccountStorageAuthHelper(Profile* profile);
  ~AccountStorageAuthHelper() = default;

  AccountStorageAuthHelper(const AccountStorageAuthHelper&) = delete;
  AccountStorageAuthHelper& operator=(const AccountStorageAuthHelper&) = delete;

  // Requests a reauth for the given |account_id|. |reauth_callback| is called
  // passing whether the reauth succeeded or not.
  // TODO(crbug.com/1070944): Set the opt-in pref internally and update comment.
  // TODO(crbug.com/1070944): Retrieve the primary account id internally and
  // update method comment.
  void TriggerOptInReauth(
      const CoreAccountId& account_id,
      base::OnceCallback<
          void(password_manager::PasswordManagerClient::ReauthSucceeded)>
          reauth_callback);

  // Redirects the user to a sign-in in a new tab.
  void TriggerSignIn();

 private:
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_STORAGE_AUTH_HELPER_H_
