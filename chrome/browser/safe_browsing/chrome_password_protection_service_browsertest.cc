// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kGaiaPasswordChangeHistogramName[] =
    "PasswordProtection.GaiaPasswordReusesBeforeGaiaPasswordChanged";
const char kLoginPageUrl[] = "/safe_browsing/login_page.html";
const char kChangePasswordUrl[] = "/safe_browsing/change_password_page.html";

}  // namespace

namespace safe_browsing {

class ChromePasswordProtectionServiceBrowserTest : public InProcessBrowserTest {
 public:
  ChromePasswordProtectionServiceBrowserTest() {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void TearDownOnMainThread() override { identity_test_env_adaptor_.reset(); }

  ChromePasswordProtectionService* GetService(bool is_incognito) {
    return ChromePasswordProtectionService::GetPasswordProtectionService(
        is_incognito ? browser()->profile()->GetOffTheRecordProfile()
                     : browser()->profile());
  }

  void SimulateGaiaPasswordChange(const std::string& new_password) {
    password_manager::HashPasswordManager hash_manager;
    hash_manager.set_prefs(browser()->profile()->GetPrefs());
    hash_manager.SavePasswordHash(user_manager::kStubUserEmail,
                                  base::UTF8ToUTF16(new_password));
  }

  void SimulateGaiaPasswordChanged(ChromePasswordProtectionService* service) {
    service->OnGaiaPasswordChanged();
  }

  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    return helper->GetSecurityLevel();
  }

  std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
      content::WebContents* web_contents) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    return helper->GetVisibleSecurityState();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(
                    &ChromePasswordProtectionServiceBrowserTest::
                        OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Makes user signed-in with the stub account's email and |hosted_domain|.
  void SetUpPrimaryAccountWithHostedDomain(const std::string& hosted_domain) {
    // Ensure that the stub user is signed in.
#if defined(OS_CHROMEOS)
    // On ChromeOS, the stub user is signed in by default on browsertests.
    CoreAccountInfo account_info =
        identity_test_env()->identity_manager()->GetPrimaryAccountInfo();
    identity_test_env()->SetRefreshTokenForPrimaryAccount();
#else
    CoreAccountInfo account_info =
        identity_test_env()->MakePrimaryAccountAvailable(
            user_manager::kStubUserEmail);
#endif
    ASSERT_EQ(account_info.email, user_manager::kStubUserEmail);

    identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        hosted_domain, "full_name", "given_name", "locale",
        "http://picture.example.com/picture.jpg");
  }

  void ConfigureEnterprisePasswordProtection(
      bool is_gsuite,
      PasswordProtectionTrigger trigger_type) {
    if (is_gsuite)
      SetUpPrimaryAccountWithHostedDomain("example.com");
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kPasswordProtectionWarningTrigger, trigger_type);
    browser()->profile()->GetPrefs()->SetString(
        prefs::kPasswordProtectionChangePasswordURL,
        embedded_test_server()->GetURL(kChangePasswordUrl).spec());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordProtectionServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyChangeSignInPassword) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  service->OnUserAction(web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
                        WarningAction::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // Verify myaccount.google.com or Google signin page should be opened in a
  // new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetVisibleURL()
                  .DomainIs("google.com"));

  // Simulates user finished changing password.
  SimulateGaiaPasswordChanged(service);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyShowWarningIncognito) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/true);
  Profile* profile = browser()->profile()->GetOffTheRecordProfile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Change password card on chrome settings page should NOT show.
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       MarkSiteAsLegitimate) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Ignore" button on the modal dialog.
  service->OnUserAction(web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
                        WarningAction::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  // No new tab opens. Security info doesn't change.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  EXPECT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OpenChromeSettingsViaPageInfo) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Simulates clicking "Ignore" to close dialog.
  service->OnUserAction(web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
                        WarningAction::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          browser()->profile()));
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents, PasswordType::PRIMARY_ACCOUNT_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  // Verify myaccount.google.com or Google signin page should be opened in a
  // new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetVisibleURL()
                  .DomainIs("google.com"));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyUnhandledPasswordReuse) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  // Prepare sync account will trigger a password change.
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ASSERT_TRUE(service);
  Profile* profile = browser()->profile();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_TRUE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
          ->empty());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  base::HistogramTester histograms;
  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Opens a new browser window.
  Browser* browser2 = CreateBrowser(profile);
  // Shows modal dialog on this new web_contents.
  content::WebContents* new_web_contents =
      browser2->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser2, GURL("data:text/html,<html></html>"));
  service->ShowModalWarning(new_web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChange("new_password");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_THAT(histograms.GetAllSamples(kGaiaPasswordChangeHistogramName),
              testing::ElementsAre(base::Bucket(2, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyCheckGaiaPasswordChange) {
  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  // Configures initial password to "password_1";
  password_manager::PasswordHashData hash_data(
      user_manager::kStubUserEmail, base::UTF8ToUTF16("password_1"), true);
  password_manager::HashPasswordManager hash_manager;
  hash_manager.set_prefs(profile->GetPrefs());
  hash_manager.SavePasswordHash(hash_data);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::PRIMARY_ACCOUNT_PASSWORD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());

  // Save the same password will not trigger OnGaiaPasswordChanged(), thus no
  // change to size of unhandled_password_reuses().
  SimulateGaiaPasswordChange("password_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());

  // Save a different password will clear unhandled_password_reuses().
  SimulateGaiaPasswordChange("password_2");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u,
            profile->GetPrefs()
                ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
                ->size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyShouldShowChangePasswordSettingUI) {
  Profile* profile = browser()->profile();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  // Simulates previous session has unhandled password reuses.
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  update->SetKey("https://oldreuse.com",
                 /*navigation_id=*/base::Value("12345"));

  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChanged(GetService(/*is_incognito=*/false));
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_TRUE(
      profile->GetPrefs()
          ->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses)
          ->empty());
}

IN_PROC_BROWSER_TEST_F(
    ChromePasswordProtectionServiceBrowserTest,
    VerifyIsPasswordReuseProtectionConfiguredForNonDomainUser) {
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  // If prefs::kPasswordProtectionWarningTrigger isn't set to PASSWORD_REUSE,
  // |IsPasswordReuseProtectionConfigured(..)| returns false.
  EXPECT_EQ(PHISHING_REUSE, service->GetPasswordProtectionWarningTriggerPref());
  EXPECT_FALSE(
      ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
          profile));

  SetUpPrimaryAccountWithHostedDomain(kNoHostedDomainFound);
  profile->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                  PasswordProtectionTrigger::PASSWORD_REUSE);
  // Otherwise, |IsPasswordReuseProtectionConfigured(..)| returns false for
  // GMAIL users.
  EXPECT_FALSE(
      ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
          profile));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyIsPasswordReuseProtectionConfiguredForDomainUser) {
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  SetUpPrimaryAccountWithHostedDomain("domain.com");
  EXPECT_EQ(PHISHING_REUSE, service->GetPasswordProtectionWarningTriggerPref());
  EXPECT_FALSE(
      ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
          profile));

  profile->GetPrefs()->SetInteger(prefs::kPasswordProtectionWarningTrigger,
                                  PasswordProtectionTrigger::PASSWORD_REUSE);
  EXPECT_TRUE(
      ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
          profile));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       ChromeEnterprisePasswordAlertMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PASSWORD_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  base::HistogramTester histograms;
  // Shows interstitial on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowInterstitial(web_contents, PasswordType::ENTERPRISE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_web_contents,
                                           /*number_of_navigations=*/1);
  observer.Wait();
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialString"),
              testing::ElementsAre(base::Bucket(2, 1)));

  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecuteScript(new_web_contents, script));
  content::TestNavigationObserver observer1(new_web_contents,
                                            /*number_of_navigations=*/1);
  observer1.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetLastCommittedURL());
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialAction."
                                       "NonGaiaEnterprisePasswordEntry"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       UserDirectlyNavigateToResetPasswordPage) {
  base::HistogramTester histograms;
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PASSWORD_REUSE);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://reset-password"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // chrome://reset-password page should be opened.
  ASSERT_EQ(GURL(chrome::kChromeUIResetPasswordURL),
            web_contents->GetVisibleURL());
  EXPECT_THAT(histograms.GetAllSamples("PasswordProtection.InterstitialString"),
              testing::ElementsAre(base::Bucket(0, 1)));

  // Clicks on "Reset Password" button.
  std::string script =
      "var node = document.getElementById('reset-password-button'); \n"
      "node.click();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  content::TestNavigationObserver observer1(web_contents,
                                            /*number_of_navigations=*/1);
  observer1.Wait();
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            embedded_test_server()->GetURL(kChangePasswordUrl));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseMode) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::ENTERPRISE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Enterprise password reuse should not trigger warning in Chrome settings UI.
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::ENTERPRISE_PASSWORD));
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  // Security info should be properly updated.
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  service->OnUserAction(web_contents, PasswordType::ENTERPRISE_PASSWORD,
                        WarningUIType::MODAL_DIALOG,
                        WarningAction::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Enterprise change password page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseMarkSiteAsLegitimate) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::ENTERPRISE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents, PasswordType::ENTERPRISE_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
          web_contents, PasswordType::ENTERPRISE_PASSWORD));
  EXPECT_EQ(security_state::NONE, GetSecurityLevel(web_contents));
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       EnterprisePhishingReuseOpenChromeSettingsViaPageInfo) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token",
                            PasswordType::ENTERPRISE_PASSWORD);
  base::RunLoop().RunUntilIdle();

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents, PasswordType::ENTERPRISE_PASSWORD,
                        WarningUIType::PAGE_INFO,
                        WarningAction::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Enterprise change password page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(embedded_test_server()->GetURL(kChangePasswordUrl),
            new_web_contents->GetVisibleURL());
  // Security info should be updated.
  ASSERT_EQ(security_state::DANGEROUS, GetSecurityLevel(web_contents));
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            GetVisibleSecurityState(web_contents)->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OnEnterpriseTriggerOffGSuite) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/true, PasswordProtectionTrigger::PHISHING_REUSE);
  Profile* profile = browser()->profile();
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile->GetPrefs());
  hash_password_manager.SavePasswordHash(service->GetAccountInfo().email,
                                         base::UTF8ToUTF16("password"),
                                         /*is_gaia_password=*/true);
  ASSERT_EQ(1u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());

  // Turn off trigger
  profile->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      service->GetAccountInfo().email, /*is_gaia_password=*/true));
  EXPECT_EQ(0u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OnEnterpriseTriggerOff) {
  ConfigureEnterprisePasswordProtection(
      /*is_gsuite=*/false, PasswordProtectionTrigger::PHISHING_REUSE);
  Profile* profile = browser()->profile();
  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile->GetPrefs());
  hash_password_manager.SavePasswordHash(
      "username", base::UTF8ToUTF16("password"), /*is_gaia_password=*/false);
  hash_password_manager.SavePasswordHash("foo@gmail.com",
                                         base::UTF8ToUTF16("password"),
                                         /*is_gaia_password=*/true);
  ASSERT_EQ(2u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());

  // Turn off trigger
  profile->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      PasswordProtectionTrigger::PASSWORD_PROTECTION_OFF);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(hash_password_manager.HasPasswordHash(
      "username", /*is_gaia_password=*/false));
  EXPECT_TRUE(hash_password_manager.HasPasswordHash("foo@gmail.com",
                                                    /*is_gaia_password=*/true));
  EXPECT_EQ(1u, profile->GetPrefs()
                    ->GetList(password_manager::prefs::kPasswordHashDataList)
                    ->GetList()
                    .size());
}

}  // namespace safe_browsing
