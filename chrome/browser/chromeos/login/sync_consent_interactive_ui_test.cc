// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"

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
  void OnConsentRecordedIds(const std::vector<int>& consent_description,
                            const int consent_confirmation) override {
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

  const std::vector<int>& get_consent_description_ids() const {
    return consent_description_ids_;
  }
  int get_consent_confirmation_id() const { return consent_confirmation_id_; }
  const ::login::StringList& get_consent_description_strings() const {
    return consent_description_strings_;
  }
  const std::string& get_consent_confirmation_string() const {
    return consent_confirmation_string_;
  }

 private:
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

}  // anonymous namespace

class SyncConsentTest : public OobeBaseTest {
 public:
  SyncConsentTest() = default;
  ~SyncConsentTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    official_build_override_ = WizardController::ForceOfficialBuildForTesting();
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

  void WaitForUIToLoad() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    observer.Wait();
  }

  void SwitchLanguage(const std::string& language) {
    const char get_num_reloads[] = "Oobe.getInstance().reloadContentNumEvents_";
    const int prev_reloads = test::OobeJS().GetInt(get_num_reloads);
    test::OobeJS().Evaluate("$('connect').applySelectedLanguage_('" + language +
                            "');");
    const std::string condition =
        base::StringPrintf("%s > %d", get_num_reloads, prev_reloads);
    test::OobeJS().CreateWaiter(condition)->Wait();
  }

  void LoginToSyncConsentScreen() {
    WizardController::default_controller()->SkipToLoginForTesting(
        LoginScreenContext());
    WaitForGaiaPageEvent("ready");
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                  FakeGaiaMixin::kFakeUserPassword,
                                  FakeGaiaMixin::kEmptyUserServices);

    test::CreateOobeScreenWaiter("sync-consent")->Wait();
  }

 protected:
  void SyncConsentRecorderTestImpl(
      const std::vector<std::string>& expected_consent_strings,
      const std::string expected_consent_confirmation_string) {
    SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            SyncConsentScreenView::kScreenId));
    ConsentRecordedWaiter consent_recorded_waiter;
    screen->SetDelegateForTesting(&consent_recorded_waiter);

    screen->SetProfileSyncDisabledByPolicyForTesting(false);
    screen->SetProfileSyncEngineInitializedForTesting(true);
    screen->OnStateChanged(nullptr);
    test::OobeJS().CreateVisibilityWaiter(true, {"sync-consent-impl"})->Wait();

    test::OobeJS().ExpectVisiblePath(
        {"sync-consent-impl", "syncConsentOverviewDialog"});
    test::OobeJS().TapOnPath(
        {"sync-consent-impl", "settingsSaveAndContinueButton"});
    consent_recorded_waiter.Wait();
    screen->SetDelegateForTesting(nullptr);  // cleanup

    const int expected_consent_confirmation_id =
        IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE;

    EXPECT_EQ(expected_consent_strings,
              consent_recorded_waiter.get_consent_description_strings());
    EXPECT_EQ(expected_consent_confirmation_string,
              consent_recorded_waiter.get_consent_confirmation_string());
    EXPECT_EQ(expected_consent_ids,
              consent_recorded_waiter.get_consent_description_ids());
    EXPECT_EQ(expected_consent_confirmation_id,
              consent_recorded_waiter.get_consent_confirmation_id());
  }

  std::vector<std::string> GetLocalizedExpectedConsentStrings() const {
    std::vector<std::string> result;
    for (const int& id : expected_consent_ids) {
      result.push_back(GetLocalizedConsentString(id));
    }
    return result;
  }

  const std::vector<int> expected_consent_ids = {
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
  };

  std::unique_ptr<base::AutoReset<bool>> official_build_override_;
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTest);
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SyncConsentRecorder) {
  WaitForUIToLoad();
  EXPECT_EQ(g_browser_process->GetApplicationLocale(), "en-US");
  LoginToSyncConsentScreen();
  // For En-US we hardcode strings here to catch string issues too.
  const std::vector<std::string> expected_consent_strings(
      {"You're signed in!", "Chrome sync",
       "Your bookmarks, history, passwords, and other settings will be synced "
       "to your Google Account so you can use them on all your devices.",
       "Personalize Google services",
       "Google may use your browsing history to personalize Search, ads, and "
       "other Google services. You can change this anytime at "
       "myaccount.google.com/activitycontrols/search",
       "Review sync options following setup", "Accept and continue"});
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
  ~SyncConsentTestWithParams() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTestWithParams);
};

IN_PROC_BROWSER_TEST_P(SyncConsentTestWithParams, SyncConsentTestWithLocale) {
  LOG(INFO) << "SyncConsentTestWithParams() started with param='" << GetParam()
            << "'";
  WaitForUIToLoad();
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
 public:
  SyncConsentPolicyDisabledTest() {
    // Assistant feature contains an OOBE page which is irrelevant for this
    // test.
    feature_list_.InitAndDisableFeature(switches::kAssistantFeature);
  }
  ~SyncConsentPolicyDisabledTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(SyncConsentPolicyDisabledTest);
};

IN_PROC_BROWSER_TEST_P(SyncConsentPolicyDisabledTest,
                       SyncConsentPolicyDisabled) {
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          SyncConsentScreenView::kScreenId));

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  // Expect for other screens to be skipped and begin user session.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  observer.Wait();
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SyncConsentPolicyDisabledTest,
                         testing::Bool());

}  // namespace chromeos
