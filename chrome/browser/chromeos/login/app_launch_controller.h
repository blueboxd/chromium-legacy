// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_CONTROLLER_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/login/app_launch_signin_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

class Profile;

namespace chromeos {

class AppLaunchSplashScreenView;
class LoginDisplayHost;
class OobeUI;

// Controller for the kiosk app launch process, responsible for
// coordinating loading the kiosk profile, launching the app, and
// updating the splash screen UI.
class AppLaunchController : public KioskProfileLoader::Delegate,
                            public KioskAppLauncher::Delegate,
                            public AppLaunchSigninScreen::Delegate,
                            public AppLaunchSplashScreenView::Delegate {
 public:
  typedef base::Callback<bool()> ReturnBoolCallback;

  AppLaunchController(const std::string& app_id,
                      LoginDisplayHost* host,
                      OobeUI* oobe_ui);

  ~AppLaunchController() override;

  // Starts launching an app.
  void StartAppLaunch(bool auto_launch);

  bool waiting_for_network() { return waiting_for_network_; }
  bool network_wait_timedout() { return network_wait_timedout_; }
  bool showing_network_dialog() { return showing_network_dialog_; }

  // AppLaunchSplashScreenView::Delegate:
  void OnConfigureNetwork() override;
  void OnCancelAppLaunch() override;
  void OnNetworkConfigRequested() override;
  void OnNetworkConfigFinished() override;
  void OnNetworkStateChanged(bool online) override;
  void OnDeletingSplashScreenView() override;
  KioskAppManagerBase::App GetAppData() override;

  // Customize controller for testing purposes.
  static void SkipSplashWaitForTesting();
  static void SetNetworkTimeoutCallbackForTesting(base::OnceClosure* callback);
  static void SetNetworkWaitForTesting(int wait_time_secs);
  static void SetCanConfigureNetworkCallbackForTesting(
      ReturnBoolCallback* callback);
  static void SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
      ReturnBoolCallback* callback);
  static void SetBlockAppLaunchForTesting(bool block);

 private:
  void ClearNetworkWaitTimer();
  void CleanUp();
  void OnNetworkWaitTimedout();

  // Whether the network could be configured during launching.
  bool CanConfigureNetwork();

  // Whether the owner password is needed to configure network.
  bool NeedOwnerAuthToConfigureNetwork();

  // Show network configuration UI if it is allowed. For consumer mode,
  // owner password might be checked before showing the network configure UI.
  void MaybeShowNetworkConfigureUI();

  // Show network configuration UI when ready (i.e. after app profile is
  // loaded).
  void ShowNetworkConfigureUIWhenReady();

  // KioskProfileLoader::Delegate overrides:
  void OnProfileLoaded(Profile* profile) override;
  void OnProfileLoadFailed(KioskAppLaunchError::Error error) override;
  void OnOldEncryptionDetected(const UserContext& user_context) override;

  // KioskAppLauncher::Delegate overrides:
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;
  bool ShouldSkipAppInstallation() const override;
  void OnAppInstalling() override;
  void OnAppPrepared() override;
  void OnAppLaunched() override;
  void OnLaunchFailed(KioskAppLaunchError::Error error) override;
  void OnAppWindowCreated() override;
  bool IsShowingNetworkConfigScreen() const override;

  // AppLaunchSigninScreen::Delegate overrides:
  void OnOwnerSigninSuccess() override;

  Profile* profile_ = nullptr;
  const std::string app_id_;
  LoginDisplayHost* host_ = nullptr;
  OobeUI* oobe_ui_ = nullptr;
  AppLaunchSplashScreenView* app_launch_splash_screen_view_ = nullptr;
  std::unique_ptr<KioskProfileLoader> kiosk_profile_loader_;
  std::unique_ptr<KioskAppLauncher> startup_app_launcher_;
  std::unique_ptr<AppLaunchSigninScreen> signin_screen_;

  bool launcher_ready_ = false;
  bool cleaned_up_ = false;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;

  base::OneShotTimer network_wait_timer_;
  bool waiting_for_network_ = false;
  bool network_wait_timedout_ = false;
  bool showing_network_dialog_ = false;
  bool network_config_requested_ = false;
  bool show_network_config_ui_after_profile_load_ = false;
  int64_t launch_splash_start_time_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_CONTROLLER_H_
