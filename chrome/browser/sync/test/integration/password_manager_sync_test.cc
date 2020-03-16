// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_storage/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;

MATCHER_P2(MatchesLogin, username, password, "") {
  return arg->username_value == base::UTF8ToUTF16(username) &&
         arg->password_value == base::UTF8ToUTF16(password);
}

// Note: This helper applies to ChromeOS too, but is currently unused there. So
// define it out to prevent a compile error due to the unused function.
#if !defined(OS_CHROMEOS)
void GetNewTab(Browser* browser, content::WebContents** web_contents) {
  PasswordManagerBrowserTestBase::GetNewTab(browser, web_contents);
}
#endif  // !defined(OS_CHROMEOS)

// This test fixture is similar to SingleClientPasswordsSyncTest, but it also
// sets up all the necessary test hooks etc for PasswordManager code (like
// PasswordManagerBrowserTestBase), to allow for integration tests covering
// both Sync and the PasswordManager.
class PasswordManagerSyncTest : public SyncTest {
 public:
  PasswordManagerSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);
  }

  ~PasswordManagerSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();

    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);

    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(&PasswordManagerSyncTest::
                                        OnWillCreateBrowserContextServices));
  }

  static void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    // Use TestPasswordStore to remove a possible race. Normally the
    // PasswordStore does its database manipulation on a background thread,
    // which creates a possible race during navigation. Specifically the
    // PasswordManager will ignore any forms in a page if the load from the
    // PasswordStore has not completed.
    // TODO(crbug.com/1058339): Investigate whether these test doubles are
    // really required, or whether we can use the real stores and add some
    // waiting logic.
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&password_manager::BuildPasswordStoreWithArgs<
                                content::BrowserContext,
                                password_manager::TestPasswordStore, bool>,
                            /*is_account_store=*/false));
    AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&password_manager::BuildPasswordStoreWithArgs<
                                content::BrowserContext,
                                password_manager::TestPasswordStore, bool>,
                            /*is_account_store=*/true));
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    SyncTest::TearDownOnMainThread();
  }

  // Synchronously reads all credentials from the profile password store and
  // returns them.
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  GetAllLoginsFromProfilePasswordStore() {
    scoped_refptr<password_manager::PasswordStore> password_store =
        passwords_helper::GetPasswordStore(0);
    PasswordStoreResultsObserver syncer;
    password_store->GetAllLoginsWithAffiliationAndBrandingInformation(&syncer);
    return syncer.WaitForResults();
  }

  // Synchronously reads all credentials from the account password store and
  // returns them.
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  GetAllLoginsFromAccountPasswordStore() {
    scoped_refptr<password_manager::PasswordStore> password_store =
        passwords_helper::GetAccountPasswordStore(0);
    PasswordStoreResultsObserver syncer;
    password_store->GetAllLoginsWithAffiliationAndBrandingInformation(&syncer);
    return syncer.WaitForResults();
  }

  void NavigateToFile(content::WebContents* web_contents,
                      const std::string& path) {
    ASSERT_EQ(web_contents,
              GetBrowser(0)->tab_strip_model()->GetActiveWebContents());
    NavigationObserver observer(web_contents);
    GURL url = embedded_test_server()->GetURL(path);
    ui_test_utils::NavigateToURL(GetBrowser(0), url);
    observer.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, ChooseDestinationStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Setup Sync for a secondary account (i.e. in transport mode).
  secondary_account_helper::SignInSecondaryAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Let the user opt in to the passwords account storage, and wait for it to
  // become active.
  password_manager_util::SetAccountStorageOptIn(GetProfile(0)->GetPrefs(),
                                                GetSyncService(0), true);
  PasswordSyncActiveChecker(GetSyncService(0)).Wait();
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));

  // Part 1: Save a password; it should go into the account store by default.
  {
    // Navigate to a page with a password form.
    NavigateToFile(web_contents, "/password/password_form.html");

    // Fill out and submit the password form.
    NavigationObserver observer(web_contents);
    std::string fill_and_submit =
        "document.getElementById('username_field').value = 'accountuser';"
        "document.getElementById('password_field').value = 'accountpass';"
        "document.getElementById('input_submit_button').click()";
    ASSERT_TRUE(content::ExecJs(web_contents, fill_and_submit));
    observer.Wait();

    // Save the password and check the store.
    BubbleObserver bubble_observer(web_contents);
    ASSERT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
    bubble_observer.AcceptSavePrompt();

    std::vector<std::unique_ptr<autofill::PasswordForm>> account_credentials =
        GetAllLoginsFromAccountPasswordStore();
    EXPECT_THAT(account_credentials,
                ElementsAre(MatchesLogin("accountuser", "accountpass")));
  }

  // Part 2: Mimic the user choosing to save locally; now a newly saved password
  // should end up in the profile store.
  password_manager_util::SetDefaultPasswordStore(
      GetProfile(0)->GetPrefs(), GetSyncService(0),
      autofill::PasswordForm::Store::kProfileStore);
  {
    // Navigate to a page with a password form.
    // TODO(crbug.com/1058339): If we use the same URL as in part 1 here, then
    // the test fails because the *account* data gets filled and submitted
    // again. This is because the password manager is "smart" and prefers
    // user-typed values (including autofilled-on-pageload ones) over
    // script-provided values, see
    // https://cs.chromium.org/chromium/src/components/autofill/content/renderer/form_autofill_util.cc?rcl=e38f0c99fe45ef81bd09d97f235c3dee64e2bd9f&l=1749
    // and
    // https://cs.chromium.org/chromium/src/components/autofill/content/renderer/password_autofill_agent.cc?rcl=63830d3f4b7f5fceec9609d83cf909d0cad04bb2&l=1855
    // Some PasswordManager browser tests work around this by disabling
    // autofill on pageload, see PasswordManagerBrowserTestWithAutofillDisabled.
    // NavigateToFile(web_contents, "/password/password_form.html");
    NavigateToFile(web_contents, "/password/simple_password.html");

    // Fill out and submit the password form.
    NavigationObserver observer(web_contents);
    std::string fill_and_submit =
        "document.getElementById('username_field').value = 'localuser';"
        "document.getElementById('password_field').value = 'localpass';"
        "document.getElementById('input_submit_button').click()";
    ASSERT_TRUE(content::ExecJs(web_contents, fill_and_submit));
    observer.Wait();

    // Save the password and check the store.
    BubbleObserver bubble_observer(web_contents);
    ASSERT_TRUE(bubble_observer.IsSavePromptShownAutomatically());
    bubble_observer.AcceptSavePrompt();

    std::vector<std::unique_ptr<autofill::PasswordForm>> profile_credentials =
        GetAllLoginsFromProfilePasswordStore();
    EXPECT_THAT(profile_credentials,
                ElementsAre(MatchesLogin("localuser", "localpass")));
  }
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace
