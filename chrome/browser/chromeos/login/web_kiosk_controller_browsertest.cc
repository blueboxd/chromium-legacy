// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/web_app/mock_web_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/web_kiosk_controller.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/fake_app_launch_splash_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {

class WebKioskControllerTest : public InProcessBrowserTest {
 public:
  using AppState = WebKioskController::AppState;
  using NetworkUIState = WebKioskController::NetworkUIState;

  WebKioskControllerTest() = default;
  WebKioskControllerTest(const WebKioskControllerTest&) = delete;
  WebKioskControllerTest& operator=(const WebKioskControllerTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto app_launcher = std::make_unique<MockWebKioskAppLauncher>();
    view_ = std::make_unique<FakeAppLaunchSplashScreenHandler>();
    app_launcher_ = app_launcher.get();
    controller_ = WebKioskController::CreateForTesting(view_.get(),
                                                       std::move(app_launcher));
  }

  WebKioskController* controller() { return controller_.get(); }

  WebKioskAppLauncher::Delegate* launch_controls() {
    return static_cast<WebKioskAppLauncher::Delegate*>(controller_.get());
  }

  UserSessionManagerDelegate* session_controls() {
    return static_cast<UserSessionManagerDelegate*>(controller_.get());
  }

  AppLaunchSplashScreenView::Delegate* view_controls() {
    return static_cast<AppLaunchSplashScreenView::Delegate*>(controller_.get());
  }

  MockWebKioskAppLauncher* launcher() { return app_launcher_; }

  void ExpectState(AppState app_state, NetworkUIState network_state) {
    EXPECT_EQ(app_state, controller_->app_state_);
    EXPECT_EQ(network_state, controller_->network_ui_state_);
  }

  void FireSplashScreenTimer() { controller_->OnTimerFire(); }

  void SetOnline(bool online) {
    view_->SetNetworkReady(online);
    static_cast<AppLaunchSplashScreenView::Delegate*>(controller_.get())
        ->OnNetworkStateChanged(online);
  }

  Profile* profile() { return browser()->profile(); }

  FakeAppLaunchSplashScreenHandler* view() { return view_.get(); }

 private:
  std::unique_ptr<FakeAppLaunchSplashScreenHandler> view_;
  MockWebKioskAppLauncher* app_launcher_;  // owned by |controller_|.
  std::unique_ptr<WebKioskController> controller_;
};

IN_PROC_BROWSER_TEST_F(WebKioskControllerTest, RegularFlow) {
  controller()->StartWebKiosk(EmptyAccountId());
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize(_)).Times(1);
  session_controls()->OnProfilePrepared(profile(), false);

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::NOT_SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppStartedInstalling();

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_F(WebKioskControllerTest, AlreadyInstalled) {
  controller()->StartWebKiosk(EmptyAccountId());
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize(_)).Times(1);
  session_controls()->OnProfilePrepared(profile(), false);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_F(WebKioskControllerTest, ConfigureNetworkBeforeProfile) {
  controller()->StartWebKiosk(EmptyAccountId());
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  // User presses the hotkey.
  view_controls()->OnNetworkConfigRequested();
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NEED_TO_SHOW);

  EXPECT_CALL(*launcher(), Initialize(_)).Times(1);
  session_controls()->OnProfilePrepared(profile(), false);
  // WebKioskAppLauncher::Initialize call is synchronous, we have to call the
  // response now.
  launch_controls()->InitializeNetwork();

  ExpectState(AppState::INIT_NETWORK, NetworkUIState::SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  launch_controls()->OnAppPrepared();

  // Skipping INSTALLED state since there splash screen timer is stopped when
  // network configure ui was shown.

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_F(WebKioskControllerTest,
                       ConfigureNetworkDuringInstallation) {
  controller()->StartWebKiosk(EmptyAccountId());
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize(_)).Times(1);
  session_controls()->OnProfilePrepared(profile(), false);

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::NOT_SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppStartedInstalling();

  // User presses the hotkey, current installation is canceled.
  EXPECT_CALL(*launcher(), CancelCurrentInstallation()).Times(1);
  view_controls()->OnNetworkConfigRequested();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::SHOWING);

  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  launch_controls()->OnAppStartedInstalling();
  ExpectState(AppState::INSTALLING, NetworkUIState::NOT_SHOWING);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_F(WebKioskControllerTest,
                       ConnectionLostDuringInstallation) {
  controller()->StartWebKiosk(EmptyAccountId());
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize(_)).Times(1);
  session_controls()->OnProfilePrepared(profile(), false);

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::NOT_SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppStartedInstalling();

  EXPECT_CALL(*launcher(), CancelCurrentInstallation()).Times(1);
  SetOnline(false);
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::SHOWING);

  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  launch_controls()->OnAppStartedInstalling();
  ExpectState(AppState::INSTALLING, NetworkUIState::NOT_SHOWING);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

}  // namespace chromeos
