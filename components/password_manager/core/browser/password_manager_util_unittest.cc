// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
constexpr char kTestProxyOrigin[] = "http://proxy.com/";
constexpr char kTestProxySignonRealm[] = "proxy.com/realm";
constexpr char kTestURL[] = "https://example.com/login/";
constexpr char kTestUsername[] = "Username";
constexpr char kTestUsername2[] = "Username2";
constexpr char kTestPassword[] = "12345";

autofill::PasswordForm GetTestAndroidCredential() {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.origin = GURL(kTestAndroidRealm);
  form.signon_realm = kTestAndroidRealm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

autofill::PasswordForm GetTestCredential() {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.origin = GURL(kTestURL);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

autofill::PasswordForm GetTestProxyCredential() {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::Scheme::kBasic;
  form.origin = GURL(kTestProxyOrigin);
  form.signon_realm = kTestProxySignonRealm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

}  // namespace

using password_manager::UnorderedPasswordFormElementsAre;
using testing::_;
using testing::DoAll;
using testing::Return;

TEST(PasswordManagerUtil, TrimUsernameOnlyCredentials) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms;
  std::vector<std::unique_ptr<autofill::PasswordForm>> expected_forms;
  forms.push_back(
      std::make_unique<autofill::PasswordForm>(GetTestAndroidCredential()));
  expected_forms.push_back(
      std::make_unique<autofill::PasswordForm>(GetTestAndroidCredential()));

  autofill::PasswordForm username_only;
  username_only.scheme = autofill::PasswordForm::Scheme::kUsernameOnly;
  username_only.signon_realm = kTestAndroidRealm;
  username_only.username_value = base::ASCIIToUTF16(kTestUsername2);
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));

  username_only.federation_origin =
      url::Origin::Create(GURL(kTestFederationURL));
  username_only.skip_zero_click = false;
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));
  username_only.skip_zero_click = true;
  expected_forms.push_back(
      std::make_unique<autofill::PasswordForm>(username_only));

  TrimUsernameOnlyCredentials(&forms);

  EXPECT_THAT(forms, UnorderedPasswordFormElementsAre(&expected_forms));
}

TEST(PasswordManagerUtil, GetSignonRealmWithProtocolExcluded) {
  autofill::PasswordForm http_form;
  http_form.origin = GURL("http://www.google.com/page-1/");
  http_form.signon_realm = "http://www.google.com/";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(http_form), "www.google.com/");

  autofill::PasswordForm https_form;
  https_form.origin = GURL("https://www.google.com/page-1/");
  https_form.signon_realm = "https://www.google.com/";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(https_form), "www.google.com/");

  autofill::PasswordForm federated_form;
  federated_form.origin = GURL("http://localhost:8000/");
  federated_form.signon_realm =
      "federation://localhost/accounts.federation.com";
  EXPECT_EQ(GetSignonRealmWithProtocolExcluded(federated_form),
            "localhost/accounts.federation.com");
}

TEST(PasswordManagerUtil, FindBestMatches) {
  const base::Time kNow = base::Time::Now();
  const base::Time kYesterday = kNow - base::TimeDelta::FromDays(1);
  const base::Time k2DaysAgo = kNow - base::TimeDelta::FromDays(2);
  const int kNotFound = -1;
  struct TestMatch {
    bool is_psl_match;
    base::Time date_last_used;
    std::string username;
  };
  struct TestCase {
    const char* description;
    std::vector<TestMatch> matches;
    int expected_preferred_match_index;
    std::map<std::string, size_t> expected_best_matches_indices;
  } test_cases[] = {
      {"Empty matches", {}, kNotFound, {}},
      {"1 non-psl match",
       {{.is_psl_match = false, .date_last_used = kNow, .username = "u"}},
       0,
       {{"u", 0}}},
      {"1 psl match",
       {{.is_psl_match = true, .date_last_used = kNow, .username = "u"}},
       0,
       {{"u", 0}}},
      {"2 matches with the same username",
       {{.is_psl_match = false, .date_last_used = kNow, .username = "u"},
        {.is_psl_match = false, .date_last_used = kYesterday, .username = "u"}},
       0,
       {{"u", 0}}},
      {"2 matches with different usernames, most recently used taken",
       {{.is_psl_match = false, .date_last_used = kNow, .username = "u1"},
        {.is_psl_match = false,
         .date_last_used = kYesterday,
         .username = "u2"}},
       0,
       {{"u1", 0}, {"u2", 1}}},
      {"2 matches with different usernames, non-psl much taken",
       {{.is_psl_match = false, .date_last_used = kYesterday, .username = "u1"},
        {.is_psl_match = true, .date_last_used = kNow, .username = "u2"}},
       0,
       {{"u1", 0}, {"u2", 1}}},
      {"8 matches, 3 usernames",
       {{.is_psl_match = false, .date_last_used = kYesterday, .username = "u2"},
        {.is_psl_match = true, .date_last_used = kYesterday, .username = "u3"},
        {.is_psl_match = true, .date_last_used = kYesterday, .username = "u1"},
        {.is_psl_match = false, .date_last_used = k2DaysAgo, .username = "u3"},
        {.is_psl_match = true, .date_last_used = kNow, .username = "u1"},
        {.is_psl_match = false, .date_last_used = kNow, .username = "u2"},
        {.is_psl_match = true, .date_last_used = kYesterday, .username = "u3"},
        {.is_psl_match = false, .date_last_used = k2DaysAgo, .username = "u1"}},
       5,
       {{"u1", 7}, {"u2", 5}, {"u3", 3}}},

  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.description);
    // Convert TestMatch to PasswordForm.
    std::vector<PasswordForm> owning_matches;
    for (const TestMatch& match : test_case.matches) {
      PasswordForm form;
      form.is_public_suffix_match = match.is_psl_match;
      form.date_last_used = match.date_last_used;
      form.username_value = base::ASCIIToUTF16(match.username);
      owning_matches.push_back(form);
    }
    std::vector<const PasswordForm*> matches;
    for (const PasswordForm& match : owning_matches)
      matches.push_back(&match);

    std::vector<const PasswordForm*> best_matches;
    const PasswordForm* preferred_match = nullptr;

    std::vector<const PasswordForm*> same_scheme_matches;
    FindBestMatches(matches, PasswordForm::Scheme::kHtml, &same_scheme_matches,
                    &best_matches, &preferred_match);

    if (test_case.expected_preferred_match_index == kNotFound) {
      // Case of empty |matches|.
      EXPECT_FALSE(preferred_match);
      EXPECT_TRUE(best_matches.empty());
    } else {
      // Check |preferred_match|.
      EXPECT_EQ(matches[test_case.expected_preferred_match_index],
                preferred_match);
      // Check best matches.
      ASSERT_EQ(test_case.expected_best_matches_indices.size(),
                best_matches.size());

      for (const PasswordForm* match : best_matches) {
        std::string username = base::UTF16ToUTF8(match->username_value);
        ASSERT_NE(test_case.expected_best_matches_indices.end(),
                  test_case.expected_best_matches_indices.find(username));
        size_t expected_index =
            test_case.expected_best_matches_indices.at(username);
        size_t actual_index = std::distance(
            matches.begin(), std::find(matches.begin(), matches.end(), match));
        EXPECT_EQ(expected_index, actual_index);
      }
    }
  }
}

TEST(PasswordManagerUtil, FindBestMatchesInProfileAndAccountStores) {
  const base::string16 kUsername1 = base::ASCIIToUTF16("Username1");
  const base::string16 kPassword1 = base::ASCIIToUTF16("Password1");
  const base::string16 kUsername2 = base::ASCIIToUTF16("Username2");
  const base::string16 kPassword2 = base::ASCIIToUTF16("Password2");

  PasswordForm form;
  form.is_public_suffix_match = false;
  form.date_last_used = base::Time::Now();

  // Add the same credentials in account and profile stores.
  PasswordForm account_form1(form);
  account_form1.username_value = kUsername1;
  account_form1.password_value = kPassword1;
  account_form1.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_form1(account_form1);
  profile_form1.in_store = PasswordForm::Store::kProfileStore;

  // Add the credentials for the same username in account and profile stores but
  // with different passwords.
  PasswordForm account_form2(form);
  account_form2.username_value = kUsername2;
  account_form2.password_value = kPassword1;
  account_form2.in_store = PasswordForm::Store::kAccountStore;

  PasswordForm profile_form2(account_form2);
  profile_form2.password_value = kPassword2;
  profile_form2.in_store = PasswordForm::Store::kProfileStore;

  std::vector<const PasswordForm*> matches;
  matches.push_back(&account_form1);
  matches.push_back(&profile_form1);
  matches.push_back(&account_form2);
  matches.push_back(&profile_form2);

  std::vector<const PasswordForm*> best_matches;
  const PasswordForm* preferred_match = nullptr;
  std::vector<const PasswordForm*> same_scheme_matches;
  FindBestMatches(matches, PasswordForm::Scheme::kHtml, &same_scheme_matches,
                  &best_matches, &preferred_match);
  // All 4 matches should be returned in best matches.
  EXPECT_EQ(best_matches.size(), 4U);
  EXPECT_NE(std::find(best_matches.begin(), best_matches.end(), &account_form1),
            best_matches.end());
  EXPECT_NE(std::find(best_matches.begin(), best_matches.end(), &account_form2),
            best_matches.end());
  EXPECT_NE(std::find(best_matches.begin(), best_matches.end(), &profile_form1),
            best_matches.end());
  EXPECT_NE(std::find(best_matches.begin(), best_matches.end(), &profile_form2),
            best_matches.end());
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsername) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.password_value = base::ASCIIToUTF16("new_password");

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_RejectUnknownUsername) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value = base::ASCIIToUTF16("other_username");

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_FederatedCredential) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.password_value.clear();
  parsed.federation_origin = url::Origin::Create(GURL(kTestFederationURL));

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsernamePSL) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_MatchUsernamePSLAnotherPassword) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.password_value = base::ASCIIToUTF16("new_password");

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_MatchUsernamePSLNewPasswordKnown) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.new_password_value = parsed.password_value;
  parsed.password_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil,
     GetMatchForUpdating_MatchUsernamePSLNewPasswordUnknown) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.new_password_value = base::ASCIIToUTF16("new_password");
  parsed.password_value.clear();

  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameFindByPassword) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameFindByPasswordPSL) {
  autofill::PasswordForm stored = GetTestCredential();
  stored.is_public_suffix_match = true;
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  EXPECT_EQ(&stored, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernameCMAPI) {
  autofill::PasswordForm stored = GetTestCredential();
  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();
  parsed.type = PasswordForm::Type::kApi;

  // In case of the Credential Management API we know for sure that the site
  // meant empty username. Don't try any other heuristics.
  EXPECT_EQ(nullptr, GetMatchForUpdating(parsed, {&stored}));
}

TEST(PasswordManagerUtil, GetMatchForUpdating_EmptyUsernamePickFirst) {
  autofill::PasswordForm stored1 = GetTestCredential();
  stored1.username_value = base::ASCIIToUTF16("Adam");
  stored1.password_value = base::ASCIIToUTF16("Adam_password");
  autofill::PasswordForm stored2 = GetTestCredential();
  stored2.username_value = base::ASCIIToUTF16("Ben");
  stored2.password_value = base::ASCIIToUTF16("Ben_password");
  autofill::PasswordForm stored3 = GetTestCredential();
  stored3.username_value = base::ASCIIToUTF16("Cindy");
  stored3.password_value = base::ASCIIToUTF16("Cindy_password");

  autofill::PasswordForm parsed = GetTestCredential();
  parsed.username_value.clear();

  // The first credential is picked (arbitrarily).
  EXPECT_EQ(&stored3,
            GetMatchForUpdating(parsed, {&stored3, &stored2, &stored1}));
}

TEST(PasswordManagerUtil, MakeNormalizedBlacklistedForm_Android) {
  autofill::PasswordForm blacklisted_credential = MakeNormalizedBlacklistedForm(
      password_manager::PasswordStore::FormDigest(GetTestAndroidCredential()));
  EXPECT_TRUE(blacklisted_credential.blacklisted_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, blacklisted_credential.scheme);
  EXPECT_EQ(kTestAndroidRealm, blacklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestAndroidRealm), blacklisted_credential.origin);
}

TEST(PasswordManagerUtil, MakeNormalizedBlacklistedForm_Html) {
  autofill::PasswordForm blacklisted_credential = MakeNormalizedBlacklistedForm(
      password_manager::PasswordStore::FormDigest(GetTestCredential()));
  EXPECT_TRUE(blacklisted_credential.blacklisted_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, blacklisted_credential.scheme);
  EXPECT_EQ(GURL(kTestURL).GetOrigin().spec(),
            blacklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestURL).GetOrigin(), blacklisted_credential.origin);
}

TEST(PasswordManagerUtil, MakeNormalizedBlacklistedForm_Proxy) {
  autofill::PasswordForm blacklisted_credential = MakeNormalizedBlacklistedForm(
      password_manager::PasswordStore::FormDigest(GetTestProxyCredential()));
  EXPECT_TRUE(blacklisted_credential.blacklisted_by_user);
  EXPECT_EQ(PasswordForm::Scheme::kBasic, blacklisted_credential.scheme);
  EXPECT_EQ(kTestProxySignonRealm, blacklisted_credential.signon_realm);
  EXPECT_EQ(GURL(kTestProxyOrigin), blacklisted_credential.origin);
}

TEST(PasswordManagerUtil, AccountStoragePerAccountSettings_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      password_manager::prefs::kAccountStoragePerAccountSettings);

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

TEST(PasswordManagerUtil, AccountStoragePerAccountSettings) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      password_manager::prefs::kAccountStoragePerAccountSettings);

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
  SetAccountStorageOptIn(&pref_service, &sync_service, true);
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

TEST(PasswordManagerUtil, SyncSuppressesAccountStorageOptIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      password_manager::prefs::kAccountStoragePerAccountSettings);

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

TEST(PasswordManagerUtil, SyncDisablesAccountStorage) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(
      password_manager::prefs::kAccountStoragePerAccountSettings);

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
  SetAccountStorageOptIn(&pref_service, &sync_service, true);
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

}  // namespace password_manager_util
