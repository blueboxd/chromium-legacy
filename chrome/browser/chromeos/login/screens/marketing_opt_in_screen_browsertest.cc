// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/marketing_backend_connector.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chromeos {

class MarketingOptInScreenTest : public OobeBaseTest {
 public:
  MarketingOptInScreenTest() {
    feature_list_.InitAndEnableFeature(
        ash::features::kHideShelfControlsInTabletMode);
  }
  ~MarketingOptInScreenTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    MarketingOptInScreen* marketing_screen = MarketingOptInScreen::Get(
        WizardController::default_controller()->screen_manager());
    marketing_screen->set_exit_callback_for_testing(base::BindRepeating(
        &MarketingOptInScreenTest::HandleScreenExit, base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        ash::prefs::kGestureEducationNotificationShown, true);
  }

  // Shows the gesture navigation screen.
  void ShowMarketingOptInScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        MarketingOptInScreenView::kScreenId);
  }

  void TapOnGetStartedAndWaitForScreenExit() {
    // Tapping the next button exits the screen.
    test::OobeJS().ExpectVisiblePath(
        {"marketing-opt-in", "marketing-opt-in-next-button"});
    test::OobeJS().TapOnPath(
        {"marketing-opt-in", "marketing-opt-in-next-button"});
    WaitForScreenExit();
  }

  void ShowAccessibilityButtonForTest() {
    MarketingOptInScreen* marketing_screen = MarketingOptInScreen::Get(
        WizardController::default_controller()->screen_manager());
    marketing_screen->SetA11yButtonVisibilityForTest(true /* shown */);
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForBackendRequest() {
    if (backend_request_performed_)
      return;
    base::RunLoop run_loop;
    backend_request_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void HandleBackendRequest(std::string country_code) {
    ASSERT_FALSE(backend_request_performed_);
    backend_request_performed_ = true;
    requested_country_code_ = country_code;
    if (backend_request_callback_)
      std::move(backend_request_callback_).Run();
  }

  std::string GetRequestedCountryCode() { return requested_country_code_; }

 private:
  void HandleScreenExit() {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;

  bool backend_request_performed_ = false;
  base::RepeatingClosure backend_request_callback_;
  std::string requested_country_code_;

  base::test::ScopedFeatureList feature_list_;
};

// Tests that the screen is visible
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ScreenVisible) {
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketingOptInOverviewDialog"});
}

// Marketing option not visible for unknown country
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest,
                       ToggleDisableForUnkownCountry) {
  base::HistogramTester histogram_tester;
  g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                              "unknown");
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle"});

  TapOnGetStartedAndWaitForScreenExit();
  // No UMA metric recording when the toggle isn't visible
  histogram_tester.ExpectTotalCount("OOBE.MarketingOptInScreen.Event", 0);
}

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, OptOutFlowWhenDefaultIsOptIn) {
  base::HistogramTester histogram_tester;
  g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                              "America/Los_Angeles");
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Default opt-in country. Toggle must visible, and checked.
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "chromebookUpdatesOption"});
  test::OobeJS().ExpectHasAttribute(
      "checked", {"marketing-opt-in", "chromebookUpdatesOption"});
  // Un-Check the opt-in toggle by clicking on it.
  test::OobeJS().ClickOnPath({"marketing-opt-in", "chromebookUpdatesOption"});
  // Ensure that the toggle is now 'unchecked'
  test::OobeJS().ExpectHasNoAttribute(
      "checked", {"marketing-opt-in", "chromebookUpdatesOption"});

  TapOnGetStartedAndWaitForScreenExit();
  // Expect the correct event to be recorded.
  histogram_tester.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.Event",
      MarketingOptInScreen::Event::kUserOptedOutWhenDefaultIsOptIn, 1);
}

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, OptInFlowWhenDefaultIsOptOut) {
  base::HistogramTester histogram_tester;
  ScopedRequestCallbackSetter callback_setter{
      std::make_unique<base::RepeatingCallback<void(std::string)>>(
          base::BindRepeating(&MarketingOptInScreenTest::HandleBackendRequest,
                              base::Unretained(this)))};

  g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                              "Canada/Atlantic");
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Default opt-out country. Toggle must visible, and not checked.
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "chromebookUpdatesOption"});
  test::OobeJS().ExpectHasNoAttribute(
      "checked", {"marketing-opt-in", "chromebookUpdatesOption"});

  // Check the opt-in toggle by clicking on it.
  test::OobeJS().ClickOnPath({"marketing-opt-in", "chromebookUpdatesOption"});

  // Ensure that the toggle is now 'checked'
  test::OobeJS().ExpectHasAttribute(
      "checked", {"marketing-opt-in", "chromebookUpdatesOption"});

  // Wait for the request to be performed and ensure that we have the correct
  // country code for Canada.
  TapOnGetStartedAndWaitForScreenExit();
  WaitForBackendRequest();
  EXPECT_EQ(GetRequestedCountryCode(), "ca");

  // Expect the correct event to be recorded.
  histogram_tester.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.Event",
      MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptOut, 1);
}

IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, OptInFlowWhenDefaultIsOptIn) {
  base::HistogramTester histogram_tester;
  ScopedRequestCallbackSetter callback_setter{
      std::make_unique<base::RepeatingCallback<void(std::string)>>(
          base::BindRepeating(&MarketingOptInScreenTest::HandleBackendRequest,
                              base::Unretained(this)))};

  g_browser_process->local_state()->SetString(prefs::kSigninScreenTimezone,
                                              "America/Los_Angeles");
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Default opt-in country. Toggle must visible, and checked.
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "chromebookUpdatesOption"});
  test::OobeJS().ExpectHasAttribute(
      "checked", {"marketing-opt-in", "chromebookUpdatesOption"});

  // Wait for the request to be performed and ensure that we have the correct
  // country code for the U.S.
  TapOnGetStartedAndWaitForScreenExit();
  WaitForBackendRequest();
  EXPECT_EQ(GetRequestedCountryCode(), "us");

  // Expect the correct event to be recorded.
  histogram_tester.ExpectUniqueSample(
      "OOBE.MarketingOptInScreen.Event",
      MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptIn, 1);
}

// Tests that the user can enable shelf navigation buttons in tablet mode from
// the screen.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, EnableShelfNavigationButtons) {
  ShowMarketingOptInScreen();
  ShowAccessibilityButtonForTest();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Tap on accessibility settings link, and wait for the accessibility settings
  // UI to show up.
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"marketing-opt-in", "marketing-opt-in-accessibility-button"})
      ->Wait();
  test::OobeJS().ClickOnPath(
      {"marketing-opt-in", "marketing-opt-in-accessibility-button"});
  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {"marketing-opt-in", "finalAccessibilityPage"})
      ->Wait();

  // Tap the shelf navigation buttons in tablet mode toggle.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"marketing-opt-in", "a11yNavButtonToggle"})
      ->Wait();
  test::OobeJS().ClickOnPath(
      {"marketing-opt-in", "a11yNavButtonToggle", "button"});

  // Go back to the first screen.
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "final-accessibility-back-button"});

  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"marketing-opt-in", "marketingOptInOverviewDialog"})
      ->Wait();

  TapOnGetStartedAndWaitForScreenExit();

  // Verify the accessibility pref for shelf navigation buttons is set.
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

// Tests that the user can exit the screen from the accessibility page.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ExitScreenFromA11yPage) {
  ShowMarketingOptInScreen();
  ShowAccessibilityButtonForTest();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Tap on accessibility settings link, and wait for the accessibility settings
  // UI to show up.
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"marketing-opt-in", "marketing-opt-in-accessibility-button"})
      ->Wait();
  test::OobeJS().ClickOnPath(
      {"marketing-opt-in", "marketing-opt-in-accessibility-button"});
  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {"marketing-opt-in", "finalAccessibilityPage"})
      ->Wait();

  // Tapping the next button exits the screen.
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "final-accessibility-next-button"});
  WaitForScreenExit();
}

}  // namespace chromeos
