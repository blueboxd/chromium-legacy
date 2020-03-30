// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/packaged_license_screen.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"

namespace chromeos {

class PackagedLicenseScreenTest : public OobeBaseTest {
 public:
  PackagedLicenseScreenTest() {}
  ~PackagedLicenseScreenTest() override = default;

  void SetUpOnMainThread() override {
    PackagedLicenseScreen* screen = static_cast<PackagedLicenseScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            PackagedLicenseView::kScreenId));
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &PackagedLicenseScreenTest::HandleScreenExit, base::Unretained(this)));
    policy::EnrollmentConfig config;
    config.is_license_packaged_with_device = true;
    WizardController::default_controller()
        ->set_prescribed_enrollment_config_for_testing(config);

    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowPackagedLicenseScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        PackagedLicenseView::kScreenId);
    OobeScreenWaiter(PackagedLicenseView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckResult(PackagedLicenseScreen::Result result) {
    EXPECT_EQ(result_, result);
  }

 private:
  void HandleScreenExit(PackagedLicenseScreen::Result result) {
    screen_exited_ = true;
    result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  PackagedLicenseScreen::Result result_;
  base::RepeatingClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(PackagedLicenseScreenTest, DontEnroll) {
  ShowPackagedLicenseScreen();

  test::OobeJS().TapOnPath({"packaged-license", "dont-enroll-button"});

  WaitForScreenExit();
  CheckResult(PackagedLicenseScreen::Result::DONT_ENROLL);
}

IN_PROC_BROWSER_TEST_F(PackagedLicenseScreenTest, Enroll) {
  ShowPackagedLicenseScreen();

  test::OobeJS().TapOnPath({"packaged-license", "enroll-button"});

  WaitForScreenExit();
  CheckResult(PackagedLicenseScreen::Result::ENROLL);
}

}  // namespace chromeos
