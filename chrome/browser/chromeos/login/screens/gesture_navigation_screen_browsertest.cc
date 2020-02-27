// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gesture_navigation_screen.h"

#include <string>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

class GestureNavigationScreenTest : public OobeBaseTest {
 public:
  GestureNavigationScreenTest() {
    feature_list_.InitAndEnableFeature(
        ash::features::kHideShelfControlsInTabletMode);
  }
  ~GestureNavigationScreenTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    GestureNavigationScreen* gesture_screen =
        static_cast<GestureNavigationScreen*>(
            WizardController::default_controller()->screen_manager()->GetScreen(
                GestureNavigationScreenView::kScreenId));
    gesture_screen->set_exit_callback_for_testing(
        base::BindRepeating(&GestureNavigationScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
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
        "gestureIntro", "gestureHome", "gestureBack", "gestureOverview"};
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

// Ensure a working flow for the gesture navigation screen.
IN_PROC_BROWSER_TEST_F(GestureNavigationScreenTest, FlowTest) {
  ShowGestureNavigationScreen();
  OobeScreenWaiter(GestureNavigationScreenView::kScreenId).Wait();

  CheckPageIsShown("gestureIntro");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-intro-next-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-next-button"});

  CheckPageIsShown("gestureBack");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-back-next-button"});

  // Now tap back buttons until intro screen is shown once again.
  CheckPageIsShown("gestureOverview");
  test::OobeJS().TapOnPath(
      {"gesture-navigation", "gesture-overview-back-button"});

  CheckPageIsShown("gestureBack");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-back-back-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-back-button"});

  // Go through flow all the way to screen exit.
  CheckPageIsShown("gestureIntro");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-intro-next-button"});

  CheckPageIsShown("gestureHome");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-home-next-button"});

  CheckPageIsShown("gestureBack");
  test::OobeJS().TapOnPath({"gesture-navigation", "gesture-back-next-button"});

  CheckPageIsShown("gestureOverview");
  test::OobeJS().TapOnPath(
      {"gesture-navigation", "gesture-overview-next-button"});

  WaitForScreenExit();
}

// Ensure the flow is skipped when in clamshell mode.
IN_PROC_BROWSER_TEST_F(GestureNavigationScreenTest, ScreenSkippedInClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the flow is skipped when spoken feedback is enabled.
IN_PROC_BROWSER_TEST_F(GestureNavigationScreenTest,
                       ScreenSkippedWithSpokenFeedbackEnabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the flow is skipped when autoclick is enabled.
IN_PROC_BROWSER_TEST_F(GestureNavigationScreenTest,
                       ScreenSkippedWithAutoclickEnabled) {
  AccessibilityManager::Get()->EnableAutoclick(true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

// Ensure the flow is skipped when switch access is enabled.
IN_PROC_BROWSER_TEST_F(GestureNavigationScreenTest,
                       ScreenSkippedWithSwitchAccessEnabled) {
  AccessibilityManager::Get()->SetSwitchAccessEnabled(true);

  ShowGestureNavigationScreen();

  WaitForScreenExit();
}

}  // namespace chromeos
