// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using testing::Contains;

namespace chromeos {
namespace {

class ConsentRecordedWaiter
    : public SyncConsentScreen::SyncConsentScreenTestDelegate {
 public:
  ConsentRecordedWaiter() = default;

  void Wait() {
    if (!consent_description_strings_.empty())
      return;

    run_loop_.Run();
  }

  // SyncConsentScreen::SyncConsentScreenTestDelegate
  void OnConsentRecordedIds(SyncConsentScreen::ConsentGiven consent_given,
                            const std::vector<int>& consent_description,
                            int consent_confirmation) override {
    consent_given_ = consent_given;
    consent_description_ids_ = consent_description;
    consent_confirmation_id_ = consent_confirmation;
  }

  void OnConsentRecordedStrings(
      const ::login::StringList& consent_description,
      const std::string& consent_confirmation) override {
    consent_description_strings_ = consent_description;
    consent_confirmation_string_ = consent_confirmation;

    // SyncConsentScreenHandler::SyncConsentScreenHandlerTestDelegate is
    // notified after SyncConsentScreen::SyncConsentScreenTestDelegate, so
    // this is the only place where we need to quit loop.
    run_loop_.Quit();
  }

  SyncConsentScreen::ConsentGiven consent_given_;
  std::vector<int> consent_description_ids_;
  int consent_confirmation_id_;

  ::login::StringList consent_description_strings_;
  std::string consent_confirmation_string_;

  base::RunLoop run_loop_;
};

std::string GetLocalizedConsentString(const int id) {
  std::string sanitized_string =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(id));
  base::ReplaceSubstringsAfterOffset(&sanitized_string, 0, "\u00A0" /* NBSP */,
                                     "&nbsp;");
  return sanitized_string;
}

class SyncConsentTest : public OobeBaseTest {
 public:
  SyncConsentTest() {
    // To reuse existing wizard controller in the flow.
    feature_list_.InitAndEnableFeature(
        chromeos::features::kOobeScreensPriority);
  }
  ~SyncConsentTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    if (features::IsSplitSettingsSyncEnabled()) {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_BROWSER_SYNC_OPTIONS,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
      };
    } else {
      expected_consent_ids_ = {
          IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER,
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
      };
    }

    ReplaceExitCallback();

    // Set up screen to be shown.
    GetSyncConsentScreen()->SetProfileSyncDisabledByPolicyForTesting(false);
    GetSyncConsentScreen()->SetProfileSyncEngineInitializedForTesting(true);
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  void SwitchLanguage(const std::string& language) {
    WelcomeScreen* welcome_screen = WelcomeScreen::Get(
        WizardController::default_controller()->screen_manager());
    test::LanguageReloadObserver observer(welcome_screen);
    test::OobeJS().SelectElementInPath(language,
                                       {"connect", "languageSelect", "select"});
    observer.Wait();
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();
  }

  void ReplaceExitCallback() {
    original_callback_ =
        GetSyncConsentScreen()->get_exit_callback_for_testing();
    GetSyncConsentScreen()->set_exit_callback_for_testing(base::BindRepeating(
        &SyncConsentTest::HandleScreenExit, base::Unretained(this)));
  }

  void LoginToSyncConsentScreen() {
    login_manager_mixin_.LoginAsNewReguarUser();
    OobeScreenExitWaiter(GaiaView::kScreenId).Wait();
    // No need to explicitly show the screen as it is the first one after login.
  }

  base::Optional<SyncConsentScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

 protected:
  static SyncConsentScreen* GetSyncConsentScreen() {
    return static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            SyncConsentScreenView::kScreenId));
  }

  void SyncConsentRecorderTestImpl(
      const std::vector<std::string>& expected_consent_strings,
      const std::string expected_consent_confirmation_string) {
    SyncConsentScreen* screen = GetSyncConsentScreen();
    ConsentRecordedWaiter consent_recorded_waiter;
    screen->SetDelegateForTesting(&consent_recorded_waiter);

    test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

    if (features::IsSplitSettingsSyncEnabled()) {
      test::OobeJS().ExpectVisiblePath(
          {"sync-consent-impl", "splitSettingsSyncConsentDialog"});
      test::OobeJS().TapOnPath(
          {"sync-consent-impl", "settingsAcceptAndContinueButton"});
    } else {
      test::OobeJS().ExpectVisiblePath(
          {"sync-consent-impl", "syncConsentOverviewDialog"});
      test::OobeJS().TapOnPath(
          {"sync-consent-impl", "settingsSaveAndContinueButton"});
    }
    consent_recorded_waiter.Wait();
    screen->SetDelegateForTesting(nullptr);  // cleanup

    const int expected_consent_confirmation_id =
        IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE;

    EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
              consent_recorded_waiter.consent_given_);
    EXPECT_EQ(expected_consent_strings,
              consent_recorded_waiter.consent_description_strings_);
    EXPECT_EQ(expected_consent_confirmation_string,
              consent_recorded_waiter.consent_confirmation_string_);
    EXPECT_EQ(expected_consent_ids_,
              consent_recorded_waiter.consent_description_ids_);
    EXPECT_EQ(expected_consent_confirmation_id,
              consent_recorded_waiter.consent_confirmation_id_);

    WaitForScreenExit();
    EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
    histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent",
                                       1);
  }

  std::vector<std::string> GetLocalizedExpectedConsentStrings() const {
    std::vector<std::string> result;
    for (const int& id : expected_consent_ids_) {
      result.push_back(GetLocalizedConsentString(id));
    }
    return result;
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void HandleScreenExit(SyncConsentScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  SyncConsentScreen::ScreenExitCallback original_callback_;

  std::vector<int> expected_consent_ids_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTest);
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedNotBrandedBuild) {
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(false);
  LoginToSyncConsentScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SkippedSyncDisabledByPolicy) {
  // Set up screen and policy.
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  SyncConsentScreen* screen = GetSyncConsentScreen();
  screen->SetProfileSyncDisabledByPolicyForTesting(true);

  LoginToSyncConsentScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 0);
}

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SyncConsentRecorder) {
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  LoginToSyncConsentScreen();
  WaitForScreenShown();
  // For En-US we hardcode strings here to catch string issues too.
  std::vector<std::string> expected_consent_strings;
  if (features::IsSplitSettingsSyncEnabled()) {
    expected_consent_strings = {
        "You're signed in!",
        "Chrome OS settings sync",
        "Your apps, settings, and other customizations will sync across all "
        "Chrome OS devices signed in with your Google Account.",
        "Chrome browser sync",
        "Your bookmarks, history, passwords, and other settings will be synced "
        "to your Google Account so you can use them on all your devices.",
        "Review browser sync options following setup",
        "Personalize Google services",
        "Google may use your browsing history to personalize Search, ads, and "
        "other Google services. You can change this anytime at "
        "myaccount.google.com/activitycontrols/search",
        "Accept and continue"};
  } else {
    expected_consent_strings = {
        "You're signed in!",
        "Chrome sync",
        "Your bookmarks, history, passwords, and other settings will be synced "
        "to your Google Account so you can use them on all your devices.",
        "Personalize Google services",
        "Google may use your browsing history to personalize Search, ads, and "
        "other Google services. You can change this anytime at "
        "myaccount.google.com/activitycontrols/search",
        "Review sync options following setup",
        "Accept and continue"};
  }
  const std::string expected_consent_confirmation_string =
      "Accept and continue";
  SyncConsentRecorderTestImpl(expected_consent_strings,
                              expected_consent_confirmation_string);
}

class SyncConsentTestWithParams
    : public SyncConsentTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  SyncConsentTestWithParams() = default;
  ~SyncConsentTestWithParams() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTestWithParams);
};

IN_PROC_BROWSER_TEST_P(SyncConsentTestWithParams, SyncConsentTestWithLocale) {
  LOG(INFO) << "SyncConsentTestWithParams() started with param='" << GetParam()
            << "'";
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  SwitchLanguage(GetParam());
  LoginToSyncConsentScreen();
  const std::vector<std::string> expected_consent_strings =
      GetLocalizedExpectedConsentStrings();
  const std::string expected_consent_confirmation_string =
      GetLocalizedConsentString(
          IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE);
  SyncConsentRecorderTestImpl(expected_consent_strings,
                              expected_consent_confirmation_string);
}

// "es" tests language switching, "en-GB" checks switching to language varants.
INSTANTIATE_TEST_SUITE_P(SyncConsentTestWithParamsImpl,
                         SyncConsentTestWithParams,
                         testing::Values("es", "en-GB"));

// Check that policy-disabled sync does not trigger SyncConsent screen.
//
// We need to check that "disabled by policy" disables SyncConsent screen
// independently from sync engine statis. So we run test twice, both for "sync
// engine not yet initialized" and "sync engine initialized" cases. Therefore
// we use WithParamInterface<bool> here.
class SyncConsentPolicyDisabledTest : public SyncConsentTest,
                                      public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(SyncConsentPolicyDisabledTest,
                       SyncConsentPolicyDisabled) {
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = GetSyncConsentScreen();

  OobeScreenExitWaiter waiter(SyncConsentScreenView::kScreenId);

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyncConsentPolicyDisabledTest,
                         testing::Bool());

// Additional tests of the consent dialog that are only applicable when the
// SplitSettingsSync flag enabled.
class SyncConsentSplitSettingsSyncTest : public SyncConsentTest {
 public:
  SyncConsentSplitSettingsSyncTest() {
    sync_feature_list_.InitAndEnableFeature(
        chromeos::features::kSplitSettingsSync);
  }
  ~SyncConsentSplitSettingsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList sync_feature_list_;
};

// Flaky failures on sanitizer builds. https://crbug.com/1054377
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_DefaultFlow DISABLED_DefaultFlow
#else
#define MAYBE_DefaultFlow DefaultFlow
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, MAYBE_DefaultFlow) {
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  LoginToSyncConsentScreen();

  // OS sync is disabled by default.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  // Wait for content to load.
  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);
  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);
  test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

  // Dialog is visible.
  test::OobeJS().ExpectVisiblePath(
      {"sync-consent-impl", "splitSettingsSyncConsentDialog"});

  // Click the continue button and wait for the JS to C++ callback.
  test::OobeJS().ClickOnPath(
      {"sync-consent-impl", "settingsAcceptAndContinueButton"});
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  // Consent was recorded for the confirmation button.
  EXPECT_EQ(SyncConsentScreen::CONSENT_GIVEN,
            consent_recorded_waiter.consent_given_);
  EXPECT_EQ("Accept and continue",
            consent_recorded_waiter.consent_confirmation_string_);
  EXPECT_EQ(IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
            consent_recorded_waiter.consent_confirmation_id_);

  // Consent was recorded for all descriptions, including the confirmation
  // button label.
  std::vector<int> expected_ids = {
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_OS_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_BROWSER_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_BROWSER_SYNC_OPTIONS,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
  };
  EXPECT_THAT(consent_recorded_waiter.consent_description_ids_,
              testing::UnorderedElementsAreArray(expected_ids));

  // Toggle button is on-by-default, so OS sync should be on.
  EXPECT_TRUE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
}

// Flaky failures on sanitizer builds. https://crbug.com/1054377
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_UserCanDisable DISABLED_UserCanDisable
#else
#define MAYBE_UserCanDisable UserCanDisable
#endif
IN_PROC_BROWSER_TEST_F(SyncConsentSplitSettingsSyncTest, MAYBE_UserCanDisable) {
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  LoginToSyncConsentScreen();

  // Wait for content to load.
  SyncConsentScreen* screen = GetSyncConsentScreen();
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);
  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);
  test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

  // Turn off the toggle.
  test::OobeJS().ClickOnPath({"sync-consent-impl", "osSyncToggle"});

  // Click the continue button and wait for the JS to C++ callback.
  test::OobeJS().ClickOnPath(
      {"sync-consent-impl", "settingsAcceptAndContinueButton"});
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);

  // OS sync is off.
  PrefService* prefs = ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), SyncConsentScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Sync-consent.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Sync-consent", 1);
}

// Tests that the SyncConsent screen performs a timezone request so that
// subsequent screens can have a timezone to work with, and that the timezone
// is properly stored in a preference.
class SyncConsentTimezoneOverride : public SyncConsentTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOobeTimezoneOverrideForTests,
                                    "TimezeonPropagationTest");
    SyncConsentTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SyncConsentTimezoneOverride, MakesTimezoneRequest) {
  auto autoreset = SyncConsentScreen::ForceBrandedBuildForTesting(true);
  LoginToSyncConsentScreen();
  EXPECT_EQ("TimezeonPropagationTest",
            g_browser_process->local_state()->GetString(
                prefs::kSigninScreenTimezone));
}

}  // namespace
}  // namespace chromeos
