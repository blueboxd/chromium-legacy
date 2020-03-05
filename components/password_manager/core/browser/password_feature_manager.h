// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

// Keeps track of which feature of PasswordManager is enabled.
class PasswordFeatureManager {
 public:
  PasswordFeatureManager() = default;
  virtual ~PasswordFeatureManager() = default;

  virtual bool IsGenerationEnabled() const = 0;

  // Whether the current signed-in user (aka unconsented primary account) has
  // opted in to use the Google account storage for passwords (as opposed to
  // local/profile storage).
  virtual bool IsOptedInForAccountStorage() const = 0;

  // Whether it makes sense to ask the user to opt-in for account-based
  // password storage. This is true if the opt-in doesn't exist yet, but all
  // other requirements are met (i.e. there is a signed-in user etc).
  virtual bool ShouldShowAccountStorageOptIn() const = 0;

  // Sets or clears the opt-in to using account storage for passwords for the
  // current signed-in user (unconsented primary account).
  virtual void SetAccountStorageOptIn(bool opt_in) = 0;

  // Whether it makes sense to ask the user about the store when saving a
  // password (i.e. profile or account store). This is true if the user has
  // opted-in already or hasn't opted in but all other requirements are met
  // (i.e. there is a signed-in user etc).
  virtual bool ShouldShowPasswordStorePicker() const = 0;

  // Sets the default password store selected by user in prefs. This store is
  // used for saving new credentials and adding blacking listing entries.
  virtual void SetDefaultPasswordStore(
      const autofill::PasswordForm::Store& store) = 0;

  // Reads the default password store from pref that was set using
  // SetDefaultPasswordStore();
  virtual autofill::PasswordForm::Store GetDefaultPasswordStore() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordFeatureManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FEATURE_MANAGER_H_
