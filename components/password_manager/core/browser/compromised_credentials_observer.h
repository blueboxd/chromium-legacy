// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_OBSERVER_H_

#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// Observes changes in Password Store logins and updates the
// CompromisedCredentialsTable accordingly.
class CompromisedCredentialsObserver : public PasswordStore::Observer {
 public:
  // Fordbid copying and assigning.
  CompromisedCredentialsObserver(const CompromisedCredentialsObserver&) =
      delete;
  CompromisedCredentialsObserver& operator=(
      const CompromisedCredentialsObserver&) = delete;

  explicit CompromisedCredentialsObserver(PasswordStore* store);
  ~CompromisedCredentialsObserver() override;

  // Adds this instance as an observer in |store_|.
  void Initialize();

 private:
  // Called when the contents of the password store change.
  // Removes rows from the compromised credentials database if the login
  // was removed or the password was updated.  If row is not in the database,
  // the call is ignored.
  void OnLoginsChanged(const PasswordStoreChangeList& changes) override;

  PasswordStore* const store_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_COMPROMISED_CREDENTIALS_OBSERVER_H_