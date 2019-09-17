// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/dns/mock_host_resolver.h"

namespace {

// The information supplied by FakeGaia for our mocked-out signin.
constexpr char kTestGaiaId[] = "12345";
constexpr char kAccountPassword[] = "fake-password";
constexpr char kTestRefreshToken[] = "fake-refresh-token";

}  // namespace

namespace policy {

class UserCloudPolicyManagerTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<std::vector<base::Feature>> {
 protected:
  UserCloudPolicyManagerTest() {
    // Override default tests configuration that prevents effective testing of
    // whether start-up URL policy is properly applied:
    // *   LoginManagerMixin skips browser launch after log-in by default.
    // *   InProcessBrowserTest force about://blank start-up URL via command
    //     line (which trumps policy values).
    login_manager_.set_should_launch_browser(true);
    set_open_about_blank_on_browser_launch(false);

    scoped_feature_list_.InitWithFeatures(
        GetParam() /* enabled_features */,
        std::vector<base::Feature>() /* disabled_features */);
  }

  ~UserCloudPolicyManagerTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    // By default, browser tests block anything that doesn't go to localhost, so
    // account.google.com requests would never reach fake GAIA server without
    // this.
    host_resolver()->AddRule("*", "127.0.0.1");
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(nullptr);
    MixinBasedInProcessBrowserTest::TearDown();
  }

  void SetMandatoryUserPolicy(const AccountId& account_id,
                              base::Value mandatory_policy) {
    ASSERT_TRUE(policy_server_.UpdateUserPolicy(
        std::move(mandatory_policy),
        base::Value(base::Value::Type::DICTIONARY) /* recommended policy */,
        account_id.GetUserEmail()));
  }

  // Sets up fake GAIA for specified user login, and requests login for the user
  // (using GaiaScreenHandler test API).
  void StartUserLogIn(const AccountId& account_id,
                      const std::string& password,
                      bool child_user) {
    fake_gaia_.SetupFakeGaiaForLogin(account_id.GetUserEmail(),
                                     account_id.GetGaiaId(), kTestRefreshToken);
    fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
        account_id.GetUserEmail(), "fake-SID-cookie", "fake-LSID-cookie");

    FakeGaia::MergeSessionParams merge_session_update;
    merge_session_update.id_token =
        child_user ? chromeos::test::GetChildAccountOAuthIdToken() : "";
    fake_gaia_.fake_gaia()->UpdateMergeSessionParams(merge_session_update);

    const std::string services =
        child_user ? chromeos::test::kChildAccountServiceFlags : "[]";
    chromeos::LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<chromeos::GaiaScreenHandler>()
        ->ShowSigninScreenForTest(account_id.GetUserEmail(), password,
                                  services);
  }

 protected:
  AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(chromeos::FakeGaiaMixin::kEnterpriseUser1,
                                     kTestGaiaId);

  // Initializing the login manager with no user will cause GAIA screen to be
  // shown on start-up.
  chromeos::LoginManagerMixin login_manager_{&mixin_host_, {}};

  chromeos::EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};
  chromeos::FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  chromeos::LocalPolicyTestServerMixin policy_server_{&mixin_host_};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerTest);
};

// TODO(agawronska): Remove test instantiation with kDMServerOAuthForChildUser
// once it is enabled by default.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerTest,
    testing::Values(std::vector<base::Feature>(),
                    std::vector<base::Feature>{
                        features::kDMServerOAuthForChildUser}));

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, StartSession) {
  // User hasn't signed in yet, so shouldn't know if the user requires policy.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kUnknown,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));

  // Set up start-up URLs through a mandatory user policy.
  const char* const kStartupURLs[] = {"chrome://policy", "chrome://about"};
  base::Value mandatory_policy(base::Value::Type::DICTIONARY);
  base::Value startup_urls(base::Value::Type::LIST);
  for (auto* const url : kStartupURLs)
    startup_urls.Append(base::Value(url));
  mandatory_policy.SetKey(key::kRestoreOnStartupURLs, std::move(startup_urls));
  mandatory_policy.SetKey(key::kRestoreOnStartup,
                          base::Value(SessionStartupPref::kPrefValueURLs));

  SetMandatoryUserPolicy(test_account_id_, std::move(mandatory_policy));

  StartUserLogIn(test_account_id_, kAccountPassword, false /*child_user*/);
  login_manager_.WaitForActiveSession();

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  const int expected_tab_count = static_cast<int>(base::size(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }

  // User should be marked as requiring policy.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));

  // It is expected that if ArcEnabled policy is not set then it is managed
  // by default and user is not able manually set it.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, ErrorLoadingPolicy) {
  policy_server_.SetExpectedPolicyFetchError(500);

  StartUserLogIn(test_account_id_, kAccountPassword, false /*child_user*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kUnknown,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest,
                       ErrorLoadingPolicyForUnmanagedUser) {
  // Mark user as not needing policy - errors loading policy should be
  // ignored (unlike previous ErrorLoadingPolicy test).
  user_manager::known_user::SetProfileRequiresPolicy(
      test_account_id_,
      user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired);

  policy_server_.SetExpectedPolicyFetchError(500);

  StartUserLogIn(test_account_id_, kAccountPassword, false /*child_user*/);
  login_manager_.WaitForActiveSession();

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // User should still be marked as not needing policy
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest,
                       NoPolicyForNonEnterpriseUser) {
  // Recognize example.com as non-enterprise account. We don't use any
  // available public domain such as gmail.com in order to prevent possible
  // leak of verification keys/signatures.
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
      "example.com");
  EXPECT_TRUE(policy::BrowserPolicyConnector::IsNonEnterpriseUser(
      test_account_id_.GetUserEmail()));
  // If a user signs in with a known non-enterprise account there should be no
  // policy.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kUnknown,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));

  StartUserLogIn(test_account_id_, kAccountPassword, false /*child_user*/);
  login_manager_.WaitForActiveSession();

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // User should be marked as not requiring policy.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, PolicyForChildUser) {
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
      "example.com");
  EXPECT_TRUE(policy::BrowserPolicyConnector::IsNonEnterpriseUser(
      test_account_id_.GetUserEmail()));

  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kUnknown,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));

  SetMandatoryUserPolicy(test_account_id_,
                         base::Value(base::Value::Type::DICTIONARY));
  StartUserLogIn(test_account_id_, kAccountPassword, true /*child_user*/);
  login_manager_.WaitForActiveSession();

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // User of CHILD type should be marked as requiring policy.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));

  // It is expected that if ArcEnabled policy is not set then it is not managed
  // by default and user is able manually set it.
  EXPECT_FALSE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, PolicyForChildUserMissing) {
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
      "example.com");
  EXPECT_TRUE(policy::BrowserPolicyConnector::IsNonEnterpriseUser(
      test_account_id_.GetUserEmail()));

  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kUnknown,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));

  StartUserLogIn(test_account_id_, kAccountPassword, true /*child_user*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(
      user_manager::known_user::ProfileRequiresPolicy::kUnknown,
      user_manager::known_user::GetProfileRequiresPolicy(test_account_id_));
}

}  // namespace policy
