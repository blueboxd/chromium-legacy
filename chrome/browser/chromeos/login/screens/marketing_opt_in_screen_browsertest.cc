// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include <string>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
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

    MarketingOptInScreen* marketing_screen = static_cast<MarketingOptInScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            MarketingOptInScreenView::kScreenId));
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

// Tests that marketing opt in toggles are hidden by default (as the command
// line switch to show marketing opt in is not set).
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, MarketingTogglesHidden) {
  ShowMarketingOptInScreen();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-subtitle"});
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle-1"});
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle-2"});

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-subtitle"});
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle-1"});
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle-2"});

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-subtitle"});
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle-1"});
  test::OobeJS().ExpectHiddenPath(
      {"marketing-opt-in", "marketing-opt-in-toggle-2"});
}

// Tests that the user can enable shelf navigation buttons in tablet mode from
// the screen.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, EnableShelfNavigationButtons) {
  ShowMarketingOptInScreen();
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

  // Tapping the next button exits the screen.
  test::OobeJS().ExpectVisiblePath(
      {"marketing-opt-in", "marketing-opt-in-next-button"});
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "marketing-opt-in-next-button"});
  WaitForScreenExit();

  // Verify the accessibility pref for shelf navigation buttons is set.
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

// Tests that the user can exit the screen from the accessibility page.
IN_PROC_BROWSER_TEST_F(MarketingOptInScreenTest, ExitScreenFromA11yPage) {
  ShowMarketingOptInScreen();
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
