// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/discover_screen.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class DiscoverScreenTest : public OobeBaseTest {
 public:
  DiscoverScreenTest() {
    // To reuse existing wizard controller in the flow.
    feature_list_.InitAndEnableFeature(
        chromeos::features::kOobeScreensPriority);
  }
  ~DiscoverScreenTest() override = default;

  void SetUpOnMainThread() override {
    DiscoverScreen* screen = static_cast<DiscoverScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            DiscoverScreenView::kScreenId));
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &DiscoverScreenTest::HandleScreenExit, base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowDiscoverScreen() {
    login_manager_mixin_.LoginAsNewReguarUser();
    OobeScreenExitWaiter(GaiaView::kScreenId).Wait();
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          DiscoverScreenView::kScreenId);
    }
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(DiscoverScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::Optional<DiscoverScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

 private:
  void HandleScreenExit(DiscoverScreen::Result result) {
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::test::ScopedFeatureList feature_list_;
  DiscoverScreen::ScreenExitCallback original_callback_;
  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DiscoverScreenTest, Skipped) {
  ShowDiscoverScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), DiscoverScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Discover.Next", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Discover", 0);
}

IN_PROC_BROWSER_TEST_F(DiscoverScreenTest, BasicFlow) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ShowDiscoverScreen();
  WaitForScreenShown();

  test::OobeJS().TapOnPath(
      {"discover-impl", "pin-setup-impl", "setupSkipButton"});

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), DiscoverScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Discover.Next", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Discover", 1);
}

}  // namespace chromeos
