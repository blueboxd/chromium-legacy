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
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/version_updater/version_updater.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/network/network_state_test_helper.h"
#include "content/public/test/browser_test.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

constexpr char kUpdateRequiredScreen[] = "update-required-card";
constexpr char kUpdateRequiredDialog[] = "update-required-dialog";
constexpr char kUpdateRequiredUpdateButton[] = "update-button";
constexpr char kUpdateProcess[] = "checking-downloading-update";
constexpr char kUpdateRequiredEolDialog[] = "eol-dialog";
constexpr char kMeteredNetworkDialog[] = "update-need-permission-dialog";
constexpr char KMeteredNetworkAcceptButton[] =
    "cellular-permission-accept-button";
constexpr char KNoNetworkDialog[] = "update-required-no-network-dialog";

// Elements in checking-downloading-update
constexpr char kUpdateProcessChecking[] = "checking-for-updates-dialog";
constexpr char kUpdateProcessUpdating[] = "updating-dialog";
constexpr char kUpdateProcessComplete[] = "update-complete-dialog";
constexpr char kCheckingForUpdatesMessage[] = "checkingForUpdatesMsg";
constexpr char kUpdatingProgress[] = "updating-progress";

constexpr char kWifiServicePath[] = "/service/wifi2";
constexpr char kCellularServicePath[] = "/service/cellular1";

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

void ErrorCallbackFunction(base::OnceClosure run_loop_quit_closure,
                           const std::string& error_name,
                           const std::string& error_message) {
  std::move(run_loop_quit_closure).Run();
  FAIL() << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  base::RunLoop run_loop;
  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path), run_loop.QuitWhenIdleClosure(),
      base::BindOnce(&ErrorCallbackFunction, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

class UpdateRequiredScreenTest : public OobeBaseTest {
 public:
  UpdateRequiredScreenTest() = default;
  ~UpdateRequiredScreenTest() override = default;
  UpdateRequiredScreenTest(const UpdateRequiredScreenTest&) = delete;
  UpdateRequiredScreenTest& operator=(const UpdateRequiredScreenTest&) = delete;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(chromeos::switches::kShillStub,
                                    "clear=1, cellular=1, wifi=1");
  }
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    tick_clock_.Advance(base::TimeDelta::FromMinutes(1));
    clock_ = std::make_unique<base::SimpleTestClock>();

    error_screen_ = GetOobeUI()->GetErrorScreen();
    update_required_screen_ = UpdateRequiredScreen::Get(
        WizardController::default_controller()->screen_manager());
    update_required_screen_->SetClockForTesting(clock_.get());

    version_updater_ = update_required_screen_->GetVersionUpdaterForTesting();
    version_updater_->set_tick_clock_for_testing(&tick_clock_);

    // Set up fake networks.
    network_state_test_helper_ =
        std::make_unique<chromeos::NetworkStateTestHelper>(
            true /*use_default_devices_and_services*/);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    // Fake networks have been set up. Connect to WiFi network.
    SetConnected(kWifiServicePath);
  }
  void TearDownOnMainThread() override {
    network_state_test_helper_.reset();

    OobeBaseTest::TearDownOnMainThread();
  }

  void SetEolDateUTC(const char* utc_date_string) {
    base::Time utc_date;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_date));
    update_engine_client()->set_eol_date(utc_date);
  }

  void SetCurrentTimeUTC(const char* utc_date_string) {
    base::Time utc_time;
    ASSERT_TRUE(base::Time::FromUTCString(utc_date_string, &utc_time));
    clock_->SetNow(utc_time);
  }

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    update_engine_client()->set_default_status(status);
    update_engine_client()->NotifyObserversThatStatusChanged(status);
  }

  void SetNetworkState(const std::string& service_path,
                       const std::string& state) {
    network_state_test_helper_->service_test()->SetServiceProperty(
        service_path, shill::kStateProperty, base::Value(state));
  }

  void ShowUpdateRequiredScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        UpdateRequiredView::kScreenId);

    OobeScreenWaiter update_screen_waiter(UpdateRequiredView::kScreenId);
    update_screen_waiter.set_assert_next_screen();
    update_screen_waiter.Wait();

    test::OobeJS().ExpectVisible(kUpdateRequiredScreen);
  }

 protected:
  UpdateRequiredScreen* update_required_screen_;
  // Error screen - owned by OobeUI.
  ErrorScreen* error_screen_ = nullptr;
  // Version updater - owned by |update_required_screen_|.
  VersionUpdater* version_updater_ = nullptr;
  // For testing captive portal
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

  base::SimpleTestTickClock tick_clock_;
  // Clock to set current time for testing EOL.
  std::unique_ptr<base::SimpleTestClock> clock_;
  // Handles network connections
  std::unique_ptr<chromeos::NetworkStateTestHelper> network_state_test_helper_;
};

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestCaptivePortal) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  update_required_screen_->SetErrorMessageDelayForTesting(
      base::TimeDelta::FromMilliseconds(10));
  ShowUpdateRequiredScreen();

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

  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredEolDialog});
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
}

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolNotReached) {
  SetEolDateUTC("1 November 2019");
  SetCurrentTimeUTC("1 January 2019");

  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredEolDialog});
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
}

// This tests the state of update required screen when the device is initially
// connected to a metered network and the user grants permission to update over
// it.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestUpdateOverMeteredNetwork) {
  // Disconnect Wifi network.
  SetNetworkState(kWifiServicePath, shill::kStateIdle);
  // Connect to cellular network and show update required screen.
  SetConnected(kCellularServicePath);

  ShowUpdateRequiredScreen();

  // Screen prompts user to either connect to a non-metered network or start
  // update over current metered network.
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kMeteredNetworkDialog});

  // Click to start update over metered network.
  test::OobeJS().TapOnPath(
      {kUpdateRequiredScreen, KMeteredNetworkAcceptButton});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kUpdateRequiredScreen, kUpdateProcess})
      ->Wait();

  // Expect screen to show progress of the update process.
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kMeteredNetworkDialog});
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the state of update required screen when the device is initially
// not connected to any network and the user connects to Wifi to start the
// update.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestUpdateRequiredNoNetwork) {
  // Disconnect from all networks and show update required screen.
  network_state_test_helper_->service_test()->ClearServices();
  base::RunLoop().RunUntilIdle();

  ShowUpdateRequiredScreen();

  // Screen shows user to connect to a network to start update.
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
  test::OobeJS().ExpectVisiblePath({kUpdateRequiredScreen, KNoNetworkDialog});

  // Connect to a WiFi network and update starts automatically.
  network_state_test_helper_->service_test()->AddService(
      kWifiServicePath, kWifiServicePath, kWifiServicePath /* name */,
      shill::kTypeWifi, shill::kStateOnline, true);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kUpdateRequiredScreen, kUpdateProcess})
      ->Wait();

  // Expect screen to show progress of the update process.
  test::OobeJS().ExpectHiddenPath({kUpdateRequiredScreen, KNoNetworkDialog});
  test::OobeJS().ExpectVisiblePath({kUpdateRequiredScreen, kUpdateProcess});

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the condition when the user switches to a metered network during
// the update process. The user then grants the permission to continue the
// update.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest,
                       TestUpdateProcessNeedPermission) {
  // Wifi is connected, show update required screen.
  ShowUpdateRequiredScreen();
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});

  // Click to start update process.
  test::OobeJS().ClickOnPath(
      {kUpdateRequiredScreen, kUpdateRequiredUpdateButton});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kUpdateRequiredScreen, kUpdateProcess})
      ->Wait();

  // Expect screen to show progress of the update process.
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
  test::OobeJS().ExpectVisiblePath({kUpdateRequiredScreen, kUpdateProcess});

  // Network changed to a metered network and update engine requires permission
  // to continue.
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);

  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {kUpdateRequiredScreen, kMeteredNetworkDialog})
      ->Wait();

  test::OobeJS().ExpectHiddenPath({kUpdateRequiredScreen, kUpdateProcess});

  // Screen prompts user to continue update on metered network. Click to
  // continue.
  test::OobeJS().TapOnPath(
      {kUpdateRequiredScreen, KMeteredNetworkAcceptButton});
  // Update process resumes.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {kUpdateRequiredScreen, kUpdateProcess})
      ->Wait();

  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kMeteredNetworkDialog});

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the state of update required screen when the device is initially
// connected to a metered network and the update process starts automatically on
// switching to a non metered network.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest,
                       TestMeteredNetworkToGoodNetwork) {
  // Disconnect from Wifi and connect to cellular network.
  SetNetworkState(kWifiServicePath, shill::kStateIdle);
  SetConnected(kCellularServicePath);

  ShowUpdateRequiredScreen();

  // Screen prompts user to either connect to a non-metered network or start
  // update over current metered network.
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kMeteredNetworkDialog});

  // Connect to a WiFi network and update starts automatically.
  SetNetworkState(kWifiServicePath, shill::kStateOnline);

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kUpdateRequiredScreen, kUpdateProcess})
      ->Wait();

  test::OobeJS().ExpectVisible(kUpdateRequiredScreen);
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kMeteredNetworkDialog});

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the update process initiated from update required screen.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestUpdateProcess) {
  // Wifi is connected, show update required screen.
  ShowUpdateRequiredScreen();
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});

  // Click to start update process.
  test::OobeJS().ClickOnPath(
      {kUpdateRequiredScreen, kUpdateRequiredUpdateButton});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {kUpdateRequiredScreen, kUpdateProcess})
      ->Wait();
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateRequiredDialog});

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  // Wait for the content of the dialog to be rendered.
  test::OobeJS()
      .CreateDisplayedWaiter(true, {kUpdateRequiredScreen, kUpdateProcess,
                                    kCheckingForUpdatesMessage})
      ->Wait();
  test::OobeJS().ExpectVisiblePath(
      {kUpdateRequiredScreen, kUpdateProcess, kUpdateProcessChecking});
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateProcess, kUpdateProcessUpdating});
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateProcess, kUpdateProcessComplete});

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  // Wait for the content of the dialog to be rendered.
  test::OobeJS()
      .CreateDisplayedWaiter(
          true, {kUpdateRequiredScreen, kUpdateProcess, kUpdatingProgress})
      ->Wait();
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateProcess, kUpdateProcessChecking});

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {kUpdateRequiredScreen, kUpdateProcess, kUpdateProcessComplete})
      ->Wait();
  test::OobeJS().ExpectHiddenPath(
      {kUpdateRequiredScreen, kUpdateProcess, kUpdateProcessUpdating});

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

}  // namespace chromeos
