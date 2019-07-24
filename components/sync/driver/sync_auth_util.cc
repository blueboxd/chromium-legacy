// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_auth_util.h"

#include <vector>

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

SyncAccountInfo::SyncAccountInfo() = default;

SyncAccountInfo::SyncAccountInfo(const CoreAccountInfo& account_info,
                                 bool is_primary)
    : account_info(account_info), is_primary(is_primary) {}

SyncAccountInfo DetermineAccountToUse(signin::IdentityManager* identity_manager,
                                      bool allow_secondary_accounts) {
  // If there is a "primary account", i.e. the user explicitly chose to
  // sign-in to Chrome, then always use that account.
  if (identity_manager->HasPrimaryAccount()) {
    return SyncAccountInfo(identity_manager->GetPrimaryAccountInfo(),
                           /*is_primary=*/true);
  }

  // Otherwise, fall back to the default content area signed-in account.
  if (allow_secondary_accounts) {
    // Check if there is a content area signed-in account, and we have a refresh
    // token for it.
    std::vector<gaia::ListedAccount> cookie_accounts =
        identity_manager->GetAccountsInCookieJar().signed_in_accounts;
    if (!cookie_accounts.empty() &&
        identity_manager->HasAccountWithRefreshToken(cookie_accounts[0].id)) {
      CoreAccountInfo account_info;
      account_info.account_id = cookie_accounts[0].id;
      account_info.gaia = cookie_accounts[0].gaia_id;
      account_info.email = cookie_accounts[0].email;
      return SyncAccountInfo(account_info, /*is_primary=*/false);
    }
  }
  return SyncAccountInfo();
}

bool IsWebSignout(const GoogleServiceAuthError& auth_error) {
  // The identity code sets an account's refresh token to be invalid (error
  // CREDENTIALS_REJECTED_BY_CLIENT) if the user signs out of that account on
  // the web.
  return auth_error ==
         GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
             GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                 CREDENTIALS_REJECTED_BY_CLIENT);
}

}  // namespace syncer
