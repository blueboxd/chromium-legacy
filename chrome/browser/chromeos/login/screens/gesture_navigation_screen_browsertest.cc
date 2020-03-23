// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gesture_navigation_screen.h"

#include <string>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

enum class TestMode { kTablet, kClamshellWithForcedTabletFirstRun };

}  // namespace

class GestureNavigationScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<TestMode> {
 public:
  GestureNavigationScreenTest() {
    feature_list_.InitAndEnableFeature(
        ash::features::kHideShelfControlsInTabletMode);
  }
  ~GestureNavigationScreenTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == TestMode::kClamshellWithForcedTabletFirstRun)
      command_line->AppendSwitch(switches::kOobeForceTabletFirstRun);
    OobeBaseTest::SetUpCommandLine(command_line);
  }
  void SetUpOnMainThread() override {
    ash::ShellTestApi().SetTabletModeEnabledForTest(StartInTabletMode());

    GestureNavigationScreen* gesture_screen =
        static_cast<GestureNavigationScreen*>(
            WizardController::default_controller()->screen_manager()->GetScreen(
                GestureNavigationScreenView::kScreenId));
    gesture_screen->set_exit_callback_for_testing(
        base::BindRepeating(&GestureNavigationScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  bool StartInTabletMode() const { return GetParam() == TestMode::kTablet; }

  bool ShouldBeSkippedInClamshell() const {
    return GetParam() != TestMode::kClamshellWithForcedTabletFirstRun;
  }

  // Shows the gesture navigation screen.
  void ShowGestureNavigationScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        GestureNavigationScreenView::kScreenId);
  }

  // Checks that |dialog_page| is shown, while also checking that all other oobe
  // dialogs on the gesture navigation screen are hidden.
  void CheckPageIsShown(std::string dialog_page) {
    // |oobe_dialogs| is a list of all pages within the gesture navigation
    // screen.
    const std::vector<std::string> oobe_dialogs = {
        "gestureIntro", "gestureHome", "gestureOverview", "gestureBack"};
    bool dialog_page_exists = false;

    for (const std::string& current_page : oobe_dialogs) {
      if (current_page == dialog_page) {
        dialog_page_exists = true;
        test::OobeJS()
            .CreateVisibilityWaiter(true, {"gesture-navigation", dialog_page})
            ->Wait();
      } else {
        test::OobeJS()
            .CreateVisibilityWaiter(false, {"gesture-navigation", current_page})
            ->Wait();
      }
    }
    EXPECT_TRUE(dialog_page_exists);
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void HandleScreenExit() {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    GestureNavigationScreenTest,
    testing::Values(TestMode::kTablet,
                    TestMode::kClamshellWithForcedTabletFirstRun));

// Ensure a working flow for the gesture navigation screen.
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest, FlowTest) {
  ShowGestureNavigationScreen();
  OobeScreenWaiter(GestureNavigationScreenView::kScreenId).Wait();

  CheckPageIsShown("gestureIntro");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-intro-next-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-next-button"});

  CheckPageIsShown("gestureOverview");
  test::OobeJS().TapOnPath(
      {"gesture-navigation", "gesture-overview-next-button"});

  // Now tap back buttons until intro screen is shown once again.
  CheckPageIsShown("gestureBack");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-back-back-button"});

  CheckPageIsShown("gestureOverview");
  test::OobeJS().TapOnPath(
      {"gesture-navigation", "gesture-overview-back-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-back-button"});

  // Go through flow all the way to screen exit.
  CheckPageIsShown("gestureIntro");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-intro-next-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-next-button"});

  CheckPageIsShown("gestureOverview");
  test::OobeJS().TapOnPath(
      {"gesture-navigation", "gesture-overview-next-button"});

  CheckPageIsShown("gestureBack");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-back-next-button"});

  WaitForScreenExit();
}

// Ensure the flow is skipped when in clamshell mode.
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest, ScreenSkippedInClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ShowGestureNavigationScreen();

  if (ShouldBeSkippedInClamshell()) {
    WaitForScreenExit();
  } else {
    OobeScreenWaiter(GestureNavigationScreenView::kScreenId).Wait();
  }
}

// Ensure the flow is skipped when spoken feedback is enabled.
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest,
                       ScreenSkippedWithSpokenFeedbackEnabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the flow is skipped when autoclick is enabled.
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest,
                       ScreenSkippedWithAutoclickEnabled) {
  AccessibilityManager::Get()->EnableAutoclick(true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the flow is skipped when switch access is enabled.
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest,
                       ScreenSkippedWithSwitchAccessEnabled) {
  AccessibilityManager::Get()->SetSwitchAccessEnabled(true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the flow is skipped when shelf navigation buttons are enabled.
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest,
                       ScreenSkippedWithShelfNavButtonsInTabletModeEnabled) {
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the page shown time metrics are being recorded during the gesture
// navigation screen flow
IN_PROC_BROWSER_TEST_P(GestureNavigationScreenTest, PageShownMetricsTest) {
  base::HistogramTester histogram_tester_;

  ShowGestureNavigationScreen();
  OobeScreenWaiter(GestureNavigationScreenView::kScreenId).Wait();

  CheckPageIsShown("gestureIntro");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-intro-next-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-next-button"});

  CheckPageIsShown("gestureOverview");
  test::OobeJS().TapOnPath(
      {"gesture-navigation", "gesture-overview-next-button"});

  CheckPageIsShown("gestureBack");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-back-next-button"});

  WaitForScreenExit();

  histogram_tester_.ExpectTotalCount(
      "OOBE.GestureNavigationScreen.PageShownTime.Intro", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.GestureNavigationScreen.PageShownTime.Home", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.GestureNavigationScreen.PageShownTime.Overview", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.GestureNavigationScreen.PageShownTime.Back", 1);
}

}  // namespace chromeos
