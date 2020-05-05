// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_features_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace features_util {
namespace {

base::Value CreateOptedInAccountPref() {
  base::Value global_pref(base::Value::Type::DICTIONARY);
  base::Value account_pref(base::Value::Type::DICTIONARY);
  account_pref.SetBoolKey("opted_in", true);
  global_pref.SetKey("some_gaia_hash", std::move(account_pref));
  return global_pref;
}
}  // namespace

TEST(PasswordFeatureManagerUtil,
     AccountStoragePerAccountSettings_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);

  CoreAccountInfo account;
  account.email = "first@account.com";
  account.gaia = "first";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // SyncService is running in transport mode with |account|.
  syncer::TestSyncService sync_service;
  sync_service.SetIsAuthenticatedAccountPrimary(false);
  sync_service.SetAuthenticatedAccountInfo(account);
  ASSERT_EQ(sync_service.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_FALSE(sync_service.IsSyncFeatureEnabled());

  // Since the account storage feature is disabled, the profile store should be
  // the default.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);

  // Same if the user is signed out.
  sync_service.SetAuthenticatedAccountInfo(CoreAccountInfo());
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);
}

TEST(PasswordFeatureManagerUtil, ShowAccountStorageResignIn) {
  TestingPrefServiceSimple pref_service;
  syncer::TestSyncService sync_service;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  // Add an account to prefs which opted into using the account-storage.
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);
  pref_service.Set(prefs::kAccountStoragePerAccountSettings,
                   CreateOptedInAccountPref());

  // SyncService is not running (because no user is signed-in).
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.SetDisableReasons(
      {syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN});

  EXPECT_TRUE(ShouldShowAccountStorageReSignin(&pref_service, &sync_service));
}

TEST(PasswordFeatureManagerUtil, ShowAccountStorageResignIn_FeatureDisabled) {
  TestingPrefServiceSimple pref_service;
  syncer::TestSyncService sync_service;
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kEnablePasswordsAccountStorage);

  // Add an account to prefs which opted into using the account-storage.
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);
  pref_service.Set(prefs::kAccountStoragePerAccountSettings,
                   CreateOptedInAccountPref());

  // SyncService is not running (because no user is signed-in).
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.SetDisableReasons(
      {syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN});

  EXPECT_FALSE(ShouldShowAccountStorageReSignin(&pref_service, &sync_service));
}

TEST(PasswordFeatureManagerUtil, DontShowAccountStorageResignIn_SyncActive) {
  TestingPrefServiceSimple pref_service;
  syncer::TestSyncService sync_service;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  // Add an account to prefs which opted into using the account-storage.
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);
  pref_service.Set(prefs::kAccountStoragePerAccountSettings,
                   CreateOptedInAccountPref());

  // SyncService is running (e.g for a different signed-in user).
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);

  EXPECT_FALSE(ShouldShowAccountStorageReSignin(&pref_service, &sync_service));
}

TEST(PasswordFeatureManagerUtil, DontShowAccountStorageResignIn_NoPrefs) {
  TestingPrefServiceSimple pref_service;
  syncer::TestSyncService sync_service;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  // Pref is registered but not set for any account.
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);

  // SyncService is not running (because no user is signed-in).
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.SetDisableReasons(
      {syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN});

  EXPECT_FALSE(ShouldShowAccountStorageReSignin(&pref_service, &sync_service));
}

TEST(PasswordFeatureManagerUtil, AccountStoragePerAccountSettings) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);

  CoreAccountInfo first_account;
  first_account.email = "first@account.com";
  first_account.gaia = "first";
  first_account.account_id = CoreAccountId::FromGaiaId(first_account.gaia);

  CoreAccountInfo second_account;
  second_account.email = "second@account.com";
  second_account.gaia = "second";
  second_account.account_id = CoreAccountId::FromGaiaId(second_account.gaia);

  syncer::TestSyncService sync_service;
  sync_service.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN});
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.SetIsAuthenticatedAccountPrimary(false);

  // Initially the user is not signed in, so everything is off/local.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowPasswordStorePicker(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);

  // Now let SyncService run in transport mode with |first_account|.
  sync_service.SetAuthenticatedAccountInfo(first_account);
  sync_service.SetDisableReasons({});
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  ASSERT_FALSE(sync_service.IsSyncFeatureEnabled());

  // By default, the user is not opted in. But since they're eligible for
  // account storage, the default store should be the account one.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_TRUE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kAccountStore);

  // Opt in!
  OptInToAccountStorage(&pref_service, &sync_service);
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  // ...and change the default store to the profile one.
  SetDefaultPasswordStore(&pref_service, &sync_service,
                          autofill::PasswordForm::Store::kProfileStore);
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);

  // Change to |second_account|. The opt-in for |first_account| should not
  // apply, and similarly the default store should be back to "account".
  sync_service.SetAuthenticatedAccountInfo(second_account);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_TRUE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kAccountStore);

  // Change back to |first_account|. The previous opt-in and chosen default
  // store should now apply again.
  sync_service.SetAuthenticatedAccountInfo(first_account);
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);

  // Sign out. Now the settings should have reasonable default values (not opted
  // in, save to profile store).
  sync_service.SetAuthenticatedAccountInfo(CoreAccountInfo());
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);
}

TEST(PasswordFeatureManagerUtil, SyncSuppressesAccountStorageOptIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);

  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // Initially, the user is signed in but doesn't have Sync-the-feature enabled,
  // so the SyncService is running in transport mode.
  syncer::TestSyncService sync_service;
  sync_service.SetIsAuthenticatedAccountPrimary(false);
  sync_service.SetAuthenticatedAccountInfo(account);
  ASSERT_EQ(sync_service.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_FALSE(sync_service.IsSyncFeatureEnabled());

  // In this state, the user could opt in to the account storage.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  ASSERT_TRUE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  ASSERT_TRUE(ShouldShowPasswordStorePicker(&pref_service, &sync_service));

  // Now the user enables Sync-the-feature.
  sync_service.SetIsAuthenticatedAccountPrimary(true);
  sync_service.SetFirstSetupComplete(true);
  ASSERT_TRUE(sync_service.IsSyncFeatureEnabled());

  // Now the account-storage opt-in should *not* be available anymore.
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowPasswordStorePicker(&pref_service, &sync_service));
}

TEST(PasswordFeatureManagerUtil, SyncDisablesAccountStorage) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);

  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in transport mode.
  syncer::TestSyncService sync_service;
  sync_service.SetIsAuthenticatedAccountPrimary(false);
  sync_service.SetAuthenticatedAccountInfo(account);
  ASSERT_EQ(sync_service.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_FALSE(sync_service.IsSyncFeatureEnabled());

  // The account storage is available in principle, so the opt-in will be shown,
  // and saving will default to the account store.
  ASSERT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  ASSERT_TRUE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  ASSERT_TRUE(ShouldShowPasswordStorePicker(&pref_service, &sync_service));
  ASSERT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kAccountStore);

  // Opt in.
  OptInToAccountStorage(&pref_service, &sync_service);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  ASSERT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  ASSERT_TRUE(ShouldShowPasswordStorePicker(&pref_service, &sync_service));
  ASSERT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kAccountStore);

  // Now enable Sync-the-feature. This should effectively turn *off* the account
  // storage again (since with Sync, there's only a single combined storage),
  // even though the opt-in wasn't actually cleared.
  sync_service.SetIsAuthenticatedAccountPrimary(true);
  sync_service.SetFirstSetupComplete(true);
  ASSERT_TRUE(sync_service.IsSyncFeatureEnabled());
  EXPECT_TRUE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowAccountStorageOptIn(&pref_service, &sync_service));
  EXPECT_FALSE(ShouldShowPasswordStorePicker(&pref_service, &sync_service));
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kProfileStore);
}

TEST(PasswordFeatureManagerUtil, OptOutClearsStorePreference) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kAccountStoragePerAccountSettings);

  CoreAccountInfo account;
  account.email = "name@account.com";
  account.gaia = "name";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  // The SyncService is running in transport mode.
  syncer::TestSyncService sync_service;
  sync_service.SetIsAuthenticatedAccountPrimary(false);
  sync_service.SetAuthenticatedAccountInfo(account);
  ASSERT_EQ(sync_service.GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  ASSERT_FALSE(sync_service.IsSyncFeatureEnabled());

  // Opt in and set default store to profile.
  OptInToAccountStorage(&pref_service, &sync_service);
  ASSERT_TRUE(IsOptedInForAccountStorage(&pref_service, &sync_service));
  SetDefaultPasswordStore(&pref_service, &sync_service,
                          autofill::PasswordForm::Store::kProfileStore);

  // Opt out.
  OptOutOfAccountStorageAndClearSettings(&pref_service, &sync_service);

  // The default store pref should have been erased, so GetDefaultPasswordStore
  // should return kAccountStore again.
  EXPECT_EQ(GetDefaultPasswordStore(&pref_service, &sync_service),
            autofill::PasswordForm::Store::kAccountStore);
  EXPECT_FALSE(IsOptedInForAccountStorage(&pref_service, &sync_service));
}

}  // namespace features_util
}  // namespace password_manager
