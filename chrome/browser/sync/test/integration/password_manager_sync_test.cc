// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

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
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/test/fake_server/fake_server_nigori_helper.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

MATCHER_P2(MatchesLogin, username, password, "") {
  return arg->username_value == base::UTF8ToUTF16(username) &&
         arg->password_value == base::UTF8ToUTF16(password);
}

MATCHER_P3(MatchesLoginAndRealm, username, password, signon_realm, "") {
  return arg->username_value == base::UTF8ToUTF16(username) &&
         arg->password_value == base::UTF8ToUTF16(password) &&
         arg->signon_realm == signon_realm;
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
    // Note: Enabling kFillOnAccountSelect effectively *disables* autofilling on
    // page load. This is important because if a password is autofilled, then
    // all Javascript changes to it are discarded, and thus any tests that cover
    // updating a password become flaky.
    feature_list_.InitWithFeatures(
        {password_manager::features::kEnablePasswordsAccountStorage,
         password_manager::features::kFillOnAccountSelect},
        {});
  }

  ~PasswordManagerSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();

    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    fake_server::SetKeystoreNigoriInFakeServer(GetFakeServer());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    SyncTest::TearDownOnMainThread();
  }

  void SetupSyncTransportWithPasswordAccountStorage() {
    // Setup Sync for a secondary account (i.e. in transport mode).
    secondary_account_helper::SignInSecondaryAccount(
        GetProfile(0), &test_url_loader_factory_, "user@email.com");
    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
    ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

    // Let the user opt in to the passwords account storage, and wait for it to
    // become active.
    password_manager::features_util::OptInToAccountStorage(
        GetProfile(0)->GetPrefs(), GetSyncService(0));
    PasswordSyncActiveChecker(GetSyncService(0)).Wait();
    ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PASSWORDS));
  }

  GURL GetWWWOrigin() {
    return embedded_test_server()->GetURL("www.example.com", "/");
  }
  GURL GetPSLOrigin() {
    return embedded_test_server()->GetURL("psl.example.com", "/");
  }

  // Returns a credential for the origin returned by GetWWWOrigin().
  autofill::PasswordForm CreateTestPasswordForm(const std::string& username,
                                                const std::string& password) {
    autofill::PasswordForm form;
    form.signon_realm = GetWWWOrigin().spec();
    form.origin = GetWWWOrigin();
    form.username_value = base::UTF8ToUTF16(username);
    form.password_value = base::UTF8ToUTF16(password);
    form.date_created = base::Time::Now();
    return form;
  }

  // Returns a credential for the origin returned by GetPSLOrigin().
  autofill::PasswordForm CreateTestPSLPasswordForm(
      const std::string& username,
      const std::string& password) {
    autofill::PasswordForm form = CreateTestPasswordForm(username, password);
    form.signon_realm = GetPSLOrigin().spec();
    form.origin = GetPSLOrigin();
    return form;
  }

  // Adds a credential to the Sync fake server for the origin returned by
  // GetWWWOrigin().
  void AddCredentialToFakeServer(const std::string& username,
                                 const std::string& password) {
    AddCredentialToFakeServer(CreateTestPasswordForm(username, password));
  }

  // Adds a credential to the Sync fake server.
  void AddCredentialToFakeServer(const autofill::PasswordForm& form) {
    passwords_helper::InjectKeystoreEncryptedServerPassword(form,
                                                            GetFakeServer());
  }

  // Adds a credential to the local store for the origin returned by
  // GetWWWOrigin().
  void AddLocalCredential(const std::string& username,
                          const std::string& password) {
    AddLocalCredential(CreateTestPasswordForm(username, password));
  }

  // Adds a credential to the local store.
  void AddLocalCredential(const autofill::PasswordForm& form) {
    scoped_refptr<password_manager::PasswordStore> password_store =
        passwords_helper::GetPasswordStore(0);
    password_store->AddLogin(form);
    // Do a roundtrip to the DB thread, to make sure the new password is stored
    // before doing anything else that might depend on it.
    GetAllLoginsFromProfilePasswordStore();
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
    GURL url = embedded_test_server()->GetURL("www.example.com", path);
    ui_test_utils::NavigateToURL(GetBrowser(0), url);
    observer.Wait();
    // After navigation, the password manager retrieves any matching credentials
    // from the store(s). So before doing anything else (like filling and
    // submitting a form), do a roundtrip to the stores to make sure the
    // credentials have finished loading.
    GetAllLoginsFromProfilePasswordStore();
    GetAllLoginsFromAccountPasswordStore();
  }

  void FillAndSubmitPasswordForm(content::WebContents* web_contents,
                                 const std::string& username,
                                 const std::string& password) {
    NavigationObserver observer(web_contents);
    std::string fill_and_submit = base::StringPrintf(
        "document.getElementById('username_field').value = '%s';"
        "document.getElementById('password_field').value = '%s';"
        "document.getElementById('input_submit_button').click()",
        username.c_str(), password.c_str());
    ASSERT_TRUE(content::ExecJs(web_contents, fill_and_submit));
    observer.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;
};

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, ChooseDestinationStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  SetupSyncTransportWithPasswordAccountStorage();

  // Part 1: Save a password; it should go into the account store by default.
  {
    // Navigate to a page with a password form, fill it out, and submit it.
    NavigateToFile(web_contents, "/password/password_form.html");
    FillAndSubmitPasswordForm(web_contents, "accountuser", "accountpass");

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
  password_manager::features_util::SetDefaultPasswordStore(
      GetProfile(0)->GetPrefs(), GetSyncService(0),
      autofill::PasswordForm::Store::kProfileStore);
  {
    // Navigate to a page with a password form, fill it out, and submit it.
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
    FillAndSubmitPasswordForm(web_contents, "localuser", "localpass");

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

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, UpdateInProfileStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in the profile store, while the account
  // store should still be empty.
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest, UpdateInAccountStore) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddCredentialToFakeServer("user", "accountpass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in the account store, while the profile
  // store should still be empty.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       UpdateMatchingCredentialInBothStores) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddCredentialToFakeServer("user", "pass");
  AddLocalCredential("user", "pass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  // TODO(crbug.com/1058339): Remove this temporary logging once the test
  // flakiness is diagnosed.
  if (!bubble_observer.IsUpdatePromptShownAutomatically()) {
    LOG(ERROR) << "ManagePasswordsUIController state: "
               << ManagePasswordsUIController::FromWebContents(web_contents)
                      ->GetState();
  }
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in both stores.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       UpdateMismatchingCredentialInBothStores) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddCredentialToFakeServer("user", "accountpass");
  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form and submit a different password.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "newpass");

  // There should be an update bubble; accept it.
  BubbleObserver bubble_observer(web_contents);
  // TODO(crbug.com/1058339): Remove this temporary logging once the test
  // flakiness is diagnosed.
  if (!bubble_observer.IsUpdatePromptShownAutomatically()) {
    LOG(ERROR) << "ManagePasswordsUIController state: "
               << ManagePasswordsUIController::FromWebContents(web_contents)
                      ->GetState();
  }
  ASSERT_TRUE(bubble_observer.IsUpdatePromptShownAutomatically());
  bubble_observer.AcceptUpdatePrompt();

  // The updated password should be in both stores.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "newpass")));
}

// Tests that if credentials for the same username, but with different passwords
// exist in the two stores, and one of them is used to successfully log in, the
// other one is silently updated to match.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdateFromAccountToProfileOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add credentials for the same username, but with different passwords, to the
  // two stores.
  AddCredentialToFakeServer("user", "accountpass");
  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  // Now we have credentials for the same user, but with different passwords, in
  // the two stores.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form and submit the version of the credentials from the profile
  // store.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "localpass");

  // Now the credential should of course still be in the profile store...
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
  // ...but also the one in the account store should have been silently updated
  // to match.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
}

// Tests that if credentials for the same username, but with different passwords
// exist in the two stores, and one of them is used to successfully log in, the
// other one is silently updated to match.
IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdateFromProfileToAccountOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add credentials for the same username, but with different passwords, to the
  // two stores.
  AddCredentialToFakeServer("user", "accountpass");
  AddLocalCredential("user", "localpass");

  SetupSyncTransportWithPasswordAccountStorage();

  // Now we have credentials for the same user, but with different passwords, in
  // the two stores.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "localpass")));
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form and submit the version of the credentials from the account
  // store.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "accountpass");

  // Now the credential should of course still be in the account store...
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));
  // ...but also the one in the profile store should have been updated to match.
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "accountpass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdatePSLMatchInAccountStoreOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the same credential to both stores, but the account one is a PSL match
  // (i.e. it's stored for psl.example.com instead of www.example.com).
  AddCredentialToFakeServer(CreateTestPSLPasswordForm("user", "pass"));
  AddLocalCredential(CreateTestPasswordForm("user", "pass"));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form (on www.) and submit it with the saved credentials.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "pass");

  // Now the PSL-matched credential should have been automatically saved for
  // www. as well (in the account store).
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
  // In the profile store, there was already a credential for www. so nothing
  // should have changed.
  ASSERT_THAT(GetAllLoginsFromProfilePasswordStore(),
              ElementsAre(MatchesLogin("user", "pass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdatePSLMatchInProfileStoreOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the same credential to both stores, but the local one is a PSL match
  // (i.e. it's stored for psl.example.com instead of www.example.com).
  AddCredentialToFakeServer(CreateTestPasswordForm("user", "pass"));
  AddLocalCredential(CreateTestPSLPasswordForm("user", "pass"));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form (on www.) and submit it with the saved credentials.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "pass");

  // Now the PSL-matched credential should have been automatically saved for
  // www. as well (in the profile store).
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
  // In the account store, there was already a credential for www. so nothing
  // should have changed.
  ASSERT_THAT(GetAllLoginsFromAccountPasswordStore(),
              ElementsAre(MatchesLogin("user", "pass")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerSyncTest,
                       AutoUpdatePSLMatchInBothStoresOnSuccessfulUse) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the same PSL-matched credential to both stores (i.e. it's stored for
  // psl.example.com instead of www.example.com).
  AddCredentialToFakeServer(CreateTestPSLPasswordForm("user", "pass"));
  AddLocalCredential(CreateTestPSLPasswordForm("user", "pass"));

  SetupSyncTransportWithPasswordAccountStorage();

  content::WebContents* web_contents = nullptr;
  GetNewTab(GetBrowser(0), &web_contents);

  // Go to a form (on www.) and submit it with the saved credentials.
  NavigateToFile(web_contents, "/password/simple_password.html");
  FillAndSubmitPasswordForm(web_contents, "user", "pass");

  // Now the PSL-matched credential should have been automatically saved for
  // www. as well, in both stores.
  EXPECT_THAT(GetAllLoginsFromAccountPasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
  EXPECT_THAT(GetAllLoginsFromProfilePasswordStore(),
              UnorderedElementsAre(
                  MatchesLoginAndRealm("user", "pass", GetWWWOrigin()),
                  MatchesLoginAndRealm("user", "pass", GetPSLOrigin())));
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace
