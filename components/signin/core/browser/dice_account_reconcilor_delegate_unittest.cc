// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(DiceAccountReconcilorDelegateTest, RevokeTokens) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  TestSigninClient client(&pref_service);
  gaia::ListedAccount gaia_account;
  gaia_account.id = CoreAccountId("other");
  {
    // Dice is enabled, revoke only tokens in error state.
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDice, /*migration_completed=*/true);
    EXPECT_EQ(
        signin::AccountReconcilorDelegate::RevokeTokenOption::kRevokeIfInError,
        delegate.ShouldRevokeSecondaryTokensBeforeReconcile(
            std::vector<gaia::ListedAccount>()));
  }
  {
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration,
        /*migration_completed=*/false);
    // Gaia accounts are not empty, don't revoke.
    gaia::ListedAccount gaia_account;
    gaia_account.id = CoreAccountId("other");
    EXPECT_EQ(AccountReconcilorDelegate::RevokeTokenOption::kDoNotRevoke,
              delegate.ShouldRevokeSecondaryTokensBeforeReconcile(
                  std::vector<gaia::ListedAccount>{gaia_account}));
    // Revoke.
    EXPECT_EQ(AccountReconcilorDelegate::RevokeTokenOption::kRevoke,
              delegate.ShouldRevokeSecondaryTokensBeforeReconcile(
                  std::vector<gaia::ListedAccount>()));
  }
}

TEST(DiceAccountReconcilorDelegateTest, ShouldRevokeTokensBasedOnCookies) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  TestSigninClient client(&pref_service);
  {
    // Dice is enabled, revoke tokens when Gaia cookie is deleted.
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDice, /*migration_completed=*/true);
    EXPECT_EQ(true, delegate.ShouldRevokeTokensOnCookieDeleted());
    EXPECT_EQ(false, delegate.ShouldRevokeTokensNotInCookies());
  }
  {
    // Dice is enabled, migration not completed, revoke tokens when
    // Gaia cookie is deleted.
    DiceAccountReconcilorDelegate delegate(&client,
                                           AccountConsistencyMethod::kDice,
                                           /*migration_completed=*/false);
    EXPECT_EQ(true, delegate.ShouldRevokeTokensOnCookieDeleted());
    EXPECT_EQ(true, delegate.ShouldRevokeTokensNotInCookies());
  }
  {
    // Dice is not enabled, do not revoke tokens when Gaia cookie is deleted.
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration,
        /*migration_completed=*/false);
    EXPECT_EQ(false, delegate.ShouldRevokeTokensOnCookieDeleted());
    EXPECT_EQ(false, delegate.ShouldRevokeTokensNotInCookies());
  }
}

}  // namespace signin
