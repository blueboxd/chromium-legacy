// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_required_screen.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/version_updater/version_updater.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"

namespace chromeos {

namespace {

constexpr char kUpdateRequiredScreen[] = "update-required-card";
constexpr char kUpdateRequiredDialog[] = "update-required-dialog";
constexpr char kUpdateRequiredUpdateButton[] = "update-button";
constexpr char kUpdateProcess[] = "checking-downloading-update";
constexpr char kUpdateRequiredEolDialog[] = "eol-dialog";

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

}  // namespace

class UpdateRequiredScreenTest : public MixinBasedInProcessBrowserTest {
 public:
  UpdateRequiredScreenTest() = default;
  ~UpdateRequiredScreenTest() override = default;
  UpdateRequiredScreenTest(const UpdateRequiredScreenTest&) = delete;
  UpdateRequiredScreenTest& operator=(const UpdateRequiredScreenTest&) = delete;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    fake_update_engine_client_ = new FakeUpdateEngineClient();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);

    tick_clock_.Advance(base::TimeDelta::FromMinutes(1));
    clock_ = std::make_unique<base::SimpleTestClock>();

    error_screen_ = GetOobeUI()->GetErrorScreen();
    update_required_screen_ = std::make_unique<UpdateRequiredScreen>(
        GetOobeUI()->GetView<UpdateRequiredScreenHandler>(), error_screen_);
    update_required_screen_->SetClockForTesting(clock_.get());

    version_updater_ = update_required_screen_->GetVersionUpdaterForTesting();
    version_updater_->set_tick_clock_for_testing(&tick_clock_);
  }

  void TearDownOnMainThread() override {
    update_required_screen_.reset();

    base::RunLoop run_loop;
    LoginDisplayHost::default_host()->Finalize(run_loop.QuitClosure());
    run_loop.Run();

    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void SetEolDateUTC(const char* utc_date_string) {
    base::Time utc_date;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_date));
    fake_update_engine_client_->set_eol_date(utc_date);
  }

  void SetCurrentTimeUTC(const char* utc_date_string) {
    base::Time utc_time;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_time));
    clock_->SetNow(utc_time);
  }

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

 protected:
  std::unique_ptr<UpdateRequiredScreen> update_required_screen_;
  // Error screen - owned by OobeUI.
  ErrorScreen* error_screen_ = nullptr;
  // Fake update engine for testing
  FakeUpdateEngineClient* fake_update_engine_client_ = nullptr;  // Unowned.
  // Version updater - owned by |update_required_screen_|.
  VersionUpdater* version_updater_ = nullptr;
  // For testing captive portal
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

  base::SimpleTestTickClock tick_clock_;
  // Clock to set current time for testing EOL.
  std::unique_ptr<base::SimpleTestClock> clock_;
};

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestCaptivePortal) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  update_required_screen_->SetErrorMessageDelayForTesting(
      base::TimeDelta::FromMilliseconds(10));
  update_required_screen_->Show();

  OobeScreenWaiter update_screen_waiter(UpdateRequiredView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible(kUpdateRequiredScreen);
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});

  // Click update button to trigger the update process.
  test::OobeJS().ClickOnPath(
      {kUpdateRequiredScreen, kUpdateRequiredUpdateButton});

  // If the network is a captive portal network, error message is shown with a
  // delay.
  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  EXPECT_EQ(UpdateRequiredView::kScreenId.AsId(),
            error_screen_->GetParentScreen());
  test::OobeJS().ExpectVisible("error-message");
  test::OobeJS().ExpectVisible("error-message-md");
  test::OobeJS().ExpectHasClass("ui-state-update", {"error-message"});
  test::OobeJS().ExpectHasClass("error-state-portal", {"error-message"});

  // If network goes back online, the error screen should be hidden and update
  // process should start.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_EQ(OobeScreen::SCREEN_UNKNOWN.AsId(),
            error_screen_->GetParentScreen());

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);

  test::OobeJS().ExpectVisible(kUpdateRequiredScreen);
  test::OobeJS().ExpectVisiblePath({kUpdateRequiredScreen, kUpdateProcess});
}

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolReached) {
  SetEolDateUTC("1 January 2019");
  SetCurrentTimeUTC("1 November 2019");
  update_required_screen_->Show();

  OobeScreenWaiter update_screen_waiter(UpdateRequiredView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible(kUpdateRequiredScreen);
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredEolDialog});
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
}

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolNotReached) {
  SetEolDateUTC("1 November 2019");
  SetCurrentTimeUTC("1 January 2019");
  update_required_screen_->Show();

  OobeScreenWaiter update_screen_waiter(UpdateRequiredView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible(kUpdateRequiredScreen);
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredEolDialog});
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
}

}  // namespace chromeos
