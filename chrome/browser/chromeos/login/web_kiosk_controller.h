// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEB_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEB_KIOSK_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chromeos/login/auth/login_performer.h"

class AccountId;
class Profile;

namespace base {
class OneShotTimer;
}

namespace chromeos {

class LoginDisplayHost;
class OobeUI;
class UserContext;

// Controller for the web kiosk launch process, responsible for loading the web
// kiosk profile, and updating the splash screen UI.
//
// Splash screen has several specific things the are considered during it's
// implementation:
// 1. There is a timer, which shows the splash screen for at least 10 seconds to
// allow used to exit the mode.
// 2. User can at any moment open network configuration menu using a shortcut
// CTRL+ALT+N to set up the network before the app actually starts.
//
// These are taken into consideration while designing the logic of
// WebKioskController.
//
// The launch process involves following steps:
// 1. Do the cryptohome login and initialize the profile.
// 2. Initialize WebKioskAppLauncher and wait for it to emit action depending on
// the current app installation state(to install a new app it is
// InitializeNetwork).
// 3. WebKioskController waits for network to be ready and when it is, it calls
// WebKioskAppLauncher::ContinueWithNetworkReady() to start the installation.
// 4. When the app is installed, we are waiting for the splash screen timer to
// finish. (We can also get here when the app is already installed).
// 5. We launch the app.
//
// At any moment the user can press the shortcut, so we need to also include the
// logic of the configuration menu display. Besides, we should be always sure
// that while configuring network we are at the correct profile. Thus, we need
// to postpone its display when the shortcut was fired too early.
//
// It is all encompassed within the combination of two states -- AppState and
// NetworkUI state.

class WebKioskController : public LoginPerformer::Delegate,
                           public UserSessionManagerDelegate,
                           public AppLaunchSplashScreenView::Delegate,
                           public WebKioskAppLauncher::Delegate {
 public:
  WebKioskController(LoginDisplayHost* host, OobeUI* oobe_ui);
  ~WebKioskController() override;

  void StartWebKiosk(const AccountId& account_id);

  static std::unique_ptr<WebKioskController> CreateForTesting(
      AppLaunchSplashScreenView* view,
      std::unique_ptr<WebKioskAppLauncher> app_launcher);

 private:
  friend class WebKioskControllerTest;

  // Used during testing.
  WebKioskController();

  // LoginPerformer::Delegate:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void WhiteListCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void SetAuthFlowOffline(bool offline) override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // UserSessionManagerDelegate:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  // AppLaunchSplashScreenView::Delegate:
  void OnCancelAppLaunch() override;
  void OnNetworkConfigRequested() override;
  void OnNetworkConfigFinished() override;
  void OnNetworkStateChanged(bool online) override;
  void OnDeletingSplashScreenView() override;
  KioskAppManagerBase::App GetAppData() override;

  // WebKioskAppLauncher:
  void InitializeNetwork() override;
  void OnAppStartedInstalling() override;
  void OnAppPrepared() override;
  void OnAppInstallFailed() override;
  void OnAppLaunched() override;
  void OnAppLaunchFailed() override;

  void CleanUp();
  void OnTimerFire();
  void CloseSplashScreen();

  // Shows network configuration dialog if kiosk profile was already created or
  // postpones the display upon creation.
  void MaybeShowNetworkConfigureUI();
  // Shows the network configuration dialog.
  void ShowNetworkConfigureUI();
  // Is fired when the controller was waiting for the network connection and
  // timed out.
  void OnNetworkWaitTimedOut();
  // Launches current app.
  void LaunchApp();

  LoginDisplayHost* const host_;  // Not owned, destructed upon shutdown.
  AppLaunchSplashScreenView* web_kiosk_splash_screen_view_;  // Owned by OobeUI.

  // Current state of the controller.
  enum class AppState {
    CREATING_PROFILE,  // Profile is being created.
    INIT_NETWORK,      // Waiting for the network to initialize.
    INSTALLING,        // App is installing.
    INSTALLED,  // App is installed, waiting for the splash screen timer to
                // fire.
    LAUNCHED    // App is being launched.
  } app_state_ = AppState::CREATING_PROFILE;

  // Current state of network configure dialog.
  enum class NetworkUIState {
    NOT_SHOWING,   // Network configure UI is not being shown.
    NEED_TO_SHOW,  // We need to show the UI as soon as we can.
    SHOWING        // Network configure UI is being shown.
  } network_ui_state_ = NetworkUIState::NOT_SHOWING;

  AccountId account_id_;

  bool launch_on_install_ = false;

  // Whether we are currently in test.
  // Disables splash screen timer and login performer.
  bool testing_ = false;

  // Used to prepare and launch the actual web kiosk app, is created after
  // profile initialization.
  std::unique_ptr<WebKioskAppLauncher> app_launcher_;
  // Used to execute login operations.
  std::unique_ptr<LoginPerformer> login_performer_;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;

  // A timer that fires when the network was not prepared and we require user
  // network configuration to continue.
  base::OneShotTimer network_wait_timer_;

  base::WeakPtrFactory<WebKioskController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebKioskController);
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEB_KIOSK_CONTROLLER_H_
