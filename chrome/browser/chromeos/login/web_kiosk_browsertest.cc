// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/kiosk_launch_controller.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/kiosk_test_helpers.h"
#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/common/web_application_info.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kAppInstallUrl[] = "https://app.com/install";
const char kAppLaunchUrl[] = "https://app.com/launch";
const char kAppTitle[] = "title.";
const test::UIPath kNetworkConfigureScreenContinueButton = {
    "error-message-md-continue-button"};

}  // namespace

class WebKioskTest : public OobeBaseTest {
 public:
  WebKioskTest()
      : account_id_(
            AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
                kAppInstallUrl,
                policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP))) {
    set_exit_when_last_browser_closes(false);
    needs_background_networking_ = true;
    skip_splash_wait_override_ =
        WebKioskController::SkipSplashScreenWaitForTesting();
    network_wait_override_ = WebKioskController::SetNetworkWaitForTesting(
        base::TimeDelta::FromSeconds(0));
  }

  WebKioskTest(const WebKioskTest&) = delete;
  WebKioskTest& operator=(const WebKioskTest&) = delete;

  void TearDownOnMainThread() override {
    settings_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  void SetOnline(bool online) {
    network_portal_detector_.SimulateDefaultNetworkState(
        online ? NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE
               : NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
  }

  const AccountId& account_id() { return account_id_; }

  void PrepareAppLaunch() {
    // Wait for the Kiosk App configuration to reload.
    content::WindowedNotificationObserver apps_loaded_signal(
        chrome::NOTIFICATION_KIOSK_APPS_LOADED,
        content::NotificationService::AllSources());
    std::vector<policy::DeviceLocalAccount> device_local_accounts = {
        policy::DeviceLocalAccount(
            policy::WebKioskAppBasicInfo(kAppInstallUrl, "", ""),
            kAppInstallUrl)};

    settings_ = std::make_unique<ScopedDeviceSettings>();
    policy::SetDeviceLocalAccounts(settings_->owner_settings_service(),
                                   device_local_accounts);
    apps_loaded_signal.Wait();
  }

  void MakeAppAlreadyInstalled() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->app_url = GURL(kAppLaunchUrl);
    info->title = base::UTF8ToUTF16(kAppTitle);
    WebKioskAppManager::Get()->UpdateAppByAccountId(account_id(),
                                                    std::move(info));
  }

  bool LaunchApp() {
    return ash::LoginScreenTestApi::LaunchApp(
        WebKioskAppManager::Get()->GetAppByAccountId(account_id())->app_id());
  }

  void SetBlockAppLaunch(bool block) {
    if (block)
      block_app_launch_override_ =
          WebKioskController::BlockAppLaunchForTesting();
    else
      block_app_launch_override_.reset();
  }

  void WaitNetworkConfigureScreenAndContinueWithOnlineState(
      bool require_network,
      bool auto_close = false) {
    SetOnline(false);
    OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
    // Unblock app launch after the network configure screen is shown.
    SetBlockAppLaunch(false);
    test::OobeJS().ExpectPathDisplayed(!require_network,
                                       kNetworkConfigureScreenContinueButton);
    SetOnline(true);

    if (!auto_close) {
      // Wait for update.
      // Continue button should be visible since we are online.
      test::OobeJS()
          .CreateDisplayedWaiter(true, kNetworkConfigureScreenContinueButton)
          ->Wait();
      test::OobeJS().ExpectPathDisplayed(true,
                                         kNetworkConfigureScreenContinueButton);
      // Click on continue button.
      test::OobeJS().TapOnPath(kNetworkConfigureScreenContinueButton);
    }
  }

 private:
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  const AccountId account_id_;
  std::unique_ptr<ScopedDeviceSettings> settings_;

  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;
  std::unique_ptr<base::AutoReset<base::TimeDelta>> network_wait_override_;
  std::unique_ptr<base::AutoReset<bool>> block_app_launch_override_;
  // Web kiosks do not support consumer-based kiosk. Network can always be
  // configured.
  ScopedCanConfigureNetwork can_configure_network_override_{true, false};
};

// Runs the kiosk app when the network is always present.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowOnline) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app when the network is not present in the beginning, but
// appears later.
IN_PROC_BROWSER_TEST_F(WebKioskTest, RegularFlowBecomesOnline) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();
  SetOnline(true);
  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app without a network connection, waits till network wait
// times out. Network configure dialog appears. Afterwards, it configures
// network and closes network configure dialog. Launch proceeds.
IN_PROC_BROWSER_TEST_F(WebKioskTest, NetworkTimeout) {
  SetOnline(false);
  PrepareAppLaunch();
  LaunchApp();

  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /*require_network*/ true, /*auto_close*/ true);

  KioskSessionInitializedWaiter().Wait();
}

// Runs the kiosk app offline when it has been already installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, AlreadyInstalledOffline) {
  SetOnline(false);
  PrepareAppLaunch();
  MakeAppAlreadyInstalled();
  LaunchApp();
  KioskSessionInitializedWaiter().Wait();
}

// Presses a network configure dialog accelerator during app launch which will
// interrupt the startup. We expect this dialog not to require network since the
// app have not yet been installed.
IN_PROC_BROWSER_TEST_F(WebKioskTest, LaunchWithConfigureAcceleratorPressed) {
  SetOnline(true);
  PrepareAppLaunch();
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  test::ExecuteOobeJS(
      "cr.ui.Oobe.handleAccelerator(\"app_launch_network_config\")");
  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ true);
  SetBlockAppLaunch(false);

  KioskSessionInitializedWaiter().Wait();
}

// In case when the app was already installed, we should expect to be able to
// configure network without need to be online.
IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       AlreadyInstalledWithConfigureAcceleratorPressed) {
  SetOnline(false);
  PrepareAppLaunch();
  MakeAppAlreadyInstalled();
  LaunchApp();

  // Block app launch after it is being installed.
  SetBlockAppLaunch(true);
  test::ExecuteOobeJS(
      "cr.ui.Oobe.handleAccelerator(\"app_launch_network_config\")");
  WaitNetworkConfigureScreenAndContinueWithOnlineState(
      /* require_network*/ false);

  KioskSessionInitializedWaiter().Wait();
}

}  // namespace chromeos
