// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_account_storage_opt_in_watcher.h"

#include <utility>

#include "base/callback.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {

PasswordAccountStorageOptInWatcher::PasswordAccountStorageOptInWatcher(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    base::RepeatingClosure change_callback)
    : identity_manager_(identity_manager),
      change_callback_(std::move(change_callback)) {
  // The opt-in state is per-account, so it can change whenever the state of the
  // signed-in account (aka unconsented primary account) changes.
  identity_manager_->AddObserver(this);
  // The opt-in state is stored in a pref, so changes to the pref might indicate
  // a change to the opt-in state.
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      password_manager::prefs::kAccountStoragePerAccountSettings,
      change_callback_);
}

PasswordAccountStorageOptInWatcher::~PasswordAccountStorageOptInWatcher() {
  identity_manager_->RemoveObserver(this);
}

void PasswordAccountStorageOptInWatcher::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  change_callback_.Run();
}

}  // namespace password_manager
