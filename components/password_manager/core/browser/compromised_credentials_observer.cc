// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_observer.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/features.h"

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

  for (const auto& change : changes) {
    if (change.password_changed()) {
      store_->RemoveCompromisedCredentials(GURL(change.form().signon_realm),
                                           change.form().username_value);
      UMA_HISTOGRAM_ENUMERATION("PasswordManager.RemoveCompromisedCredentials",
                                change.type());
    }
  }
}

}  // namespace password_manager