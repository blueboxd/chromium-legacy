// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/dbus/biod/fake_biod_client.h"

namespace chromeos {

namespace {

constexpr char kTestFingerprintDataString[] = "testFinger";

int kMaxAllowedFingerprints = 3;

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

}  // namespace

class FingerprintSetupTest : public InProcessBrowserTest {
 public:
  FingerprintSetupTest() = default;
  ~FingerprintSetupTest() override = default;

  void SetUpOnMainThread() override {
    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);

    fingerprint_setup_screen_ = std::make_unique<FingerprintSetupScreen>(
        GetOobeUI()->GetView<FingerprintSetupScreenHandler>(),
        base::BindRepeating(&FingerprintSetupTest::OnFingerprintSetupScreenExit,
                            base::Unretained(this)));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    fingerprint_setup_screen_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForScreenExit() {
    if (screen_exit_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnFingerprintSetupScreenExit() {
    screen_exit_ = true;
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  void EnrollFingerprint(int percent_complete) {
    base::RunLoop().RunUntilIdle();
    FakeBiodClient::Get()->SendEnrollScanDone(
        kTestFingerprintDataString, biod::SCAN_RESULT_SUCCESS,
        percent_complete == 100 /* is_complete */, percent_complete);
    base::RunLoop().RunUntilIdle();
  }

  void CheckCompletedEnroll() {
    test::OobeJS().ExpectVisiblePath({"fingerprint-setup-impl", "arc"});
    test::OobeJS()
        .CreateVisibilityWaiter(
            true, {"fingerprint-setup-impl", "fingerprintEnrollDone"})
        ->Wait();
    test::OobeJS().ExpectHiddenPath(
        {"fingerprint-setup-impl", "skipFingerprintEnroll"});
    test::OobeJS().ExpectVisiblePath(
        {"fingerprint-setup-impl", "arc", "checkmarkAnimation"});
    test::OobeJS().ExpectVisiblePath(
        {"fingerprint-setup-impl", "fingerprintAddAnother"});
  }

  std::unique_ptr<FingerprintSetupScreen> fingerprint_setup_screen_;

 private:
  bool screen_exit_ = false;

  base::OnceClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollHalf) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();

  EnrollFingerprint(50);
  test::OobeJS().ExpectVisiblePath({"fingerprint-setup-impl", "arc"});
  test::OobeJS().ExpectVisiblePath(
      {"fingerprint-setup-impl", "skipFingerprintEnroll"});
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "fingerprintAddAnother"});
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "fingerprintEnrollDone"});

  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "skipFingerprintEnroll"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollFull) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();

  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  EnrollFingerprint(100);
  CheckCompletedEnroll();

  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "fingerprintEnrollDone"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollLimit) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();

  for (int i = 0; i < kMaxAllowedFingerprints - 1; i++) {
    EnrollFingerprint(100);
    CheckCompletedEnroll();
    test::OobeJS().TapOnPath(
        {"fingerprint-setup-impl", "fingerprintAddAnother", "textButton"});
  }

  EnrollFingerprint(100);
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "fingerprintAddAnother"});
  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "fingerprintEnrollDone"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintDisabled) {
  quick_unlock::EnabledForTesting(false);
  fingerprint_setup_screen_->Show();

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupScreenElements) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  test::OobeJS().ExpectVisible("fingerprint-setup-impl");

  test::OobeJS().ExpectVisiblePath(
      {"fingerprint-setup-impl", "setupFingerprint"});
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupCancel) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "skipFingerprintSetup"});
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupNext) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  test::OobeJS().TapOnPath(
      {"fingerprint-setup-impl", "showSensorLocationButton"});
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"fingerprint-setup-impl", "placeFinger"})
      ->Wait();
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "setupFingerprint"});
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupLater) {
  quick_unlock::EnabledForTesting(true);
  fingerprint_setup_screen_->Show();
  OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  test::OobeJS().TapOnPath(
      {"fingerprint-setup-impl", "showSensorLocationButton"});
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"fingerprint-setup-impl", "setupFingerprintLater"})
      ->Wait();
  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "setupFingerprintLater"});

  WaitForScreenExit();
}

}  // namespace chromeos
