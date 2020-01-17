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
      !base::FeatureList::IsEnabled(password_manager::features::kLeakHistory))
    return;

  // If the change is an UPDATE and the password did not change, there is
  // nothing to remove. If the change is an ADD there is also nothing to remove.
  if (changes[0].type() == PasswordStoreChange::ADD ||
      (changes[0].type() == PasswordStoreChange::UPDATE &&
       !changes[0].password_changed())) {
    return;
  }

  // An internal update could be a (REMOVE + ADD) or (UPDATE).
  RemoveCompromisedCredentialsReason reason =
      changes.size() != 1 || changes[0].type() == PasswordStoreChange::UPDATE
          ? RemoveCompromisedCredentialsReason::kUpdate
          : RemoveCompromisedCredentialsReason::kRemove;
  store_->RemoveCompromisedCredentials(GURL(changes[0].form().signon_realm),
                                       changes[0].form().username_value,
                                       reason);
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.RemoveCompromisedCredentials",
                            changes[0].type());
}

}  // namespace password_manager