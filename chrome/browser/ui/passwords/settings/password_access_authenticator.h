// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_ACCESS_AUTHENTICATOR_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_ACCESS_AUTHENTICATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/reauth_purpose.h"

// This class takes care of reauthentication used for accessing passwords
// through the settings page. It is used on all platforms but iOS and Android
// (see //ios/chrome/browser/ui/settings/reauthentication_module.* for iOS and
// PasswordEntryEditor.java and PasswordReauthenticationFragment.java in
// chrome/android/java/src/org/chromium/chrome/browser/preferences/password/
// for Android).
class PasswordAccessAuthenticator {
 public:
  using ReauthCallback =
      base::RepeatingCallback<bool(password_manager::ReauthPurpose)>;

  // For how long after the last successful authentication a user is considered
  // authenticated without repeating the challenge.
  constexpr static int kAuthValidityPeriodSeconds = 60;

  // |os_reauth_call| is passed to |os_reauth_call_|, see the latter for
  // explanation.
  explicit PasswordAccessAuthenticator(ReauthCallback os_reauth_call);

  ~PasswordAccessAuthenticator();

  // Checks whether user is already authenticated or not, if yes,
  // execute |postAuthCallback|. Otherwise call
  // |ForceUserReauthenticationAsync| function for presenting OS
  // authentication challenge.
  // This function and |postAuthCallback| callback should be executed
  // on main thread only.
  void EnsureUserIsAuthenticatedAsync(
      password_manager::ReauthPurpose purpose,
      base::OnceCallback<void(bool)> postAuthCallback);

  // Presents the authentication challenge to the user on the
  // background thread for Windows, and on UI thread for other platforms.
  // This call is guaranteed to present the challenge to the user.
  // This function should be executed on main thread only.
  void ForceUserReauthenticationAsync(
      password_manager::ReauthPurpose purpose,
      base::OnceCallback<void(bool)> postAuthCallback);

  // Use this in tests to mock the OS-level reauthentication.
  void SetOsReauthCallForTesting(ReauthCallback os_reauth_call);

  // Use this to manipulate time in tests.
  void SetClockForTesting(base::Clock* clock);

 private:
  // Returns whether the user can skip Reauth, based on
  // |last_authentication_time_| and |kAuthValidityPeriodSeconds|
  bool CanSkipReauth() const;

  // This function will be called on main thread and it will run
  // |postAuthCallback| on main thread.
  // A successful result of |os_reauth_call_| is cached for
  // |kAuthValidityPeriodSeconds| seconds.
  void ProcessReauthenticationResult(
      password_manager::ReauthPurpose purpose,
      base::OnceCallback<void(bool)> postAuthCallback,
      bool authenticated);

  // The last time the user was successfully authenticated.
  base::Optional<base::Time> last_authentication_time_;

  // Used to measure the time since the last authentication.
  base::Clock* clock_;

  // Used to directly present the authentication challenge (such as the login
  // prompt) to the user.
  ReauthCallback os_reauth_call_;

  // Weak pointers for different callbacks.
  base::WeakPtrFactory<PasswordAccessAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessAuthenticator);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_ACCESS_AUTHENTICATOR_H_
