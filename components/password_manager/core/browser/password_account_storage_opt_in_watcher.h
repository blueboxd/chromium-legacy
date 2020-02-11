// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCOUNT_STORAGE_OPT_IN_WATCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCOUNT_STORAGE_OPT_IN_WATCHER_H_

#include "base/callback_forward.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace password_manager {

// Helper class to watch for changes to the opt-in state for the account-scoped
// password storage (see password_manager_util::IsOptedInForAccountStorage()).
class PasswordAccountStorageOptInWatcher
    : public signin::IdentityManager::Observer {
 public:
  // |identity_manager| and |pref_service| must not be null and must outlive
  // this object.
  // |change_callback| will be invoked whenever the state of
  // password_manager_util::IsOptedInForAccountStorage() might have changed.
  PasswordAccountStorageOptInWatcher(signin::IdentityManager* identity_manager,
                                     PrefService* pref_service,
                                     base::RepeatingClosure change_callback);
  ~PasswordAccountStorageOptInWatcher() override;

  // identity::IdentityManager::Observer:
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& unconsented_primary_account_info) override;

 private:
  signin::IdentityManager* const identity_manager_;
  base::RepeatingClosure change_callback_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCOUNT_STORAGE_OPT_IN_WATCHER_H_
