// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace signin {
namespace test {

// IdentityManager observer allowing to wait for sign out events for several
// accounts.
class SignOutTestObserver : public IdentityManager::Observer {
 public:
  explicit SignOutTestObserver(IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddObserver(this);
  }
  ~SignOutTestObserver() override { identity_manager_->RemoveObserver(this); }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    ++signed_out_accounts_;
    if (expected_accounts_ != -1 && signed_out_accounts_ >= expected_accounts_)
      run_loop_.Quit();
  }

  void WaitForRefreshTokenRemovedForAccounts(int expected_accounts) {
    expected_accounts_ = expected_accounts;
    if (signed_out_accounts_ >= expected_accounts_)
      return;

    run_loop_.Run();
  }

 private:
  signin::IdentityManager* identity_manager_;
  base::RunLoop run_loop_;
  int signed_out_accounts_ = 0;
  int expected_accounts_ = -1;
};

// Live tests for SignIn.
class DemoSignInTest : public signin::test::LiveTest {
 public:
  DemoSignInTest() = default;
  ~DemoSignInTest() override = default;

  void SetUp() override {
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  void SignInFromWeb(const TestAccount& test_account) {
    AddTabAtIndex(0, GaiaUrls::GetInstance()->add_account_url(),
                  ui::PageTransition::PAGE_TRANSITION_TYPED);
    SignInFromCurrentPage(test_account);
  }

  void SignInFromCurrentPage(const TestAccount& test_account) {
    TestIdentityManagerObserver observer(identity_manager());
    base::RunLoop cookie_update_loop;
    observer.SetOnAccountsInCookieUpdatedCallback(
        cookie_update_loop.QuitClosure());
    base::RunLoop refresh_token_update_loop;
    observer.SetOnRefreshTokenUpdatedCallback(
        refresh_token_update_loop.QuitClosure());
    login_ui_test_utils::ExecuteJsToSigninInSigninFrame(
        browser(), test_account.user, test_account.password);
    cookie_update_loop.Run();
    refresh_token_update_loop.Run();
  }

  void SignOutFromWeb(size_t signed_in_accounts) {
    TestIdentityManagerObserver observer(identity_manager());
    base::RunLoop cookie_update_loop;
    observer.SetOnAccountsInCookieUpdatedCallback(
        cookie_update_loop.QuitClosure());
    SignOutTestObserver sign_out_observer(identity_manager());
    AddTabAtIndex(0, GaiaUrls::GetInstance()->service_logout_url(),
                  ui::PageTransition::PAGE_TRANSITION_TYPED);
    cookie_update_loop.Run();
    sign_out_observer.WaitForRefreshTokenRemovedForAccounts(signed_in_accounts);
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }
};

// Sings in an account through the settings page and checks that the account is
// added to Chrome. Sync should be disabled.
IN_PROC_BROWSER_TEST_F(DemoSignInTest, SimpleSignInFlow) {
  GURL url("chrome://settings");
  AddTabAtIndex(0, url, ui::PageTransition::PAGE_TRANSITION_TYPED);
  auto* settings_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      settings_tab,
      "testElement = document.createElement('settings-sync-account-control');"
      "document.body.appendChild(testElement);"
      "testElement.$$('#sign-in').click();"));
  TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
  SignInFromCurrentPage(ta);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar.signed_out_accounts.empty());
  const gaia::ListedAccount& account =
      accounts_in_cookie_jar.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(ta.user, account.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
}

// Sings in two accounts on the web and checks that cookies and refresh tokens
// are added to Chrome. Sync should be disabled.
// Then, signs out on the web and checks that accounts are removed from Chrome.
IN_PROC_BROWSER_TEST_F(DemoSignInTest, WebSignInAndSignOut) {
  TestAccount test_account_1;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", test_account_1));
  SignInFromWeb(test_account_1);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_1 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_1.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar_1.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_1.signed_out_accounts.empty());
  const gaia::ListedAccount& account_1 =
      accounts_in_cookie_jar_1.signed_in_accounts[0];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_1.user, account_1.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_1.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  TestAccount test_account_2;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_2", test_account_2));
  SignInFromWeb(test_account_2);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_2 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_2.accounts_are_fresh);
  ASSERT_EQ(2u, accounts_in_cookie_jar_2.signed_in_accounts.size());
  EXPECT_TRUE(accounts_in_cookie_jar_2.signed_out_accounts.empty());
  EXPECT_EQ(accounts_in_cookie_jar_2.signed_in_accounts[0].id, account_1.id);
  const gaia::ListedAccount& account_2 =
      accounts_in_cookie_jar_2.signed_in_accounts[1];
  EXPECT_TRUE(gaia::AreEmailsSame(test_account_2.user, account_2.email));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_2.id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  SignOutFromWeb(2);

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_3 =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(accounts_in_cookie_jar_3.accounts_are_fresh);
  ASSERT_TRUE(accounts_in_cookie_jar_3.signed_in_accounts.empty());
  EXPECT_EQ(2u, accounts_in_cookie_jar_3.signed_out_accounts.size());
}

}  // namespace test
}  // namespace signin
