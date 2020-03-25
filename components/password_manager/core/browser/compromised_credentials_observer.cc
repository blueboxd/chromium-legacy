// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_observer.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/features.h"

namespace password_manager {

CompromisedCredentialsObserver::CompromisedCredentialsObserver(
    PasswordStore* store)
    : store_(store) {
  DCHECK(store_);
}

void CompromisedCredentialsObserver::Initialize() {
  store_->AddObserver(this);
}

CompromisedCredentialsObserver::~CompromisedCredentialsObserver() {
  store_->RemoveObserver(this);
}

void CompromisedCredentialsObserver::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  bool password_protection_show_domains_for_saved_password_is_on =
      base::FeatureList::IsEnabled(
          safe_browsing::kPasswordProtectionShowDomainsForSavedPasswords);
  if (!password_protection_show_domains_for_saved_password_is_on &&
      !base::FeatureList::IsEnabled(password_manager::features::kPasswordCheck))
    return;

  for (const PasswordStoreChange& change : changes) {
    // New passwords are not interesting.
    if (change.type() == PasswordStoreChange::ADD)
      continue;
    // Updates are interesting only when they change the password value.
    if (change.type() == PasswordStoreChange::UPDATE &&
        !change.password_changed())
      continue;
    auto reason = RemoveCompromisedCredentialsReason::kUpdate;
    if (change.type() == PasswordStoreChange::REMOVE &&
        std::none_of(changes.begin(), changes.end(), [](const auto& change) {
          return change.type() == PasswordStoreChange::ADD;
        })) {
      reason = RemoveCompromisedCredentialsReason::kRemove;
    }
    store_->RemoveCompromisedCredentials(change.form().signon_realm,
                                         change.form().username_value, reason);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.RemoveCompromisedCredentials",
        reason == RemoveCompromisedCredentialsReason::kUpdate
            ? PasswordStoreChange::UPDATE
            : PasswordStoreChange::REMOVE);
  }
}

}  // namespace password_manager
