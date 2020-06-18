// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ARC_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ARC_KIOSK_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

class AccountId;
class Profile;

namespace base {
class OneShotTimer;
}

namespace chromeos {

class LoginDisplayHost;
class OobeUI;
class UserContext;

// Controller for the ARC kiosk launch process, responsible for
// coordinating loading the ARC kiosk profile, and
// updating the splash screen UI.
class ArcKioskController : public KioskProfileLoader::Delegate,
                           public KioskAppLauncher::Delegate,
                           public AppLaunchSplashScreenView::Delegate {
 public:
  ArcKioskController(LoginDisplayHost* host, OobeUI* oobe_ui);
  ~ArcKioskController() override;

  // Starts ARC kiosk splash screen.
  void StartArcKiosk(const AccountId& account_id);

 private:
  void CleanUp();
  void CloseSplashScreen();

  // KioskProfileLoader:
  void OnProfileLoaded(Profile* profile) override;
  void OnProfileLoadFailed(KioskAppLaunchError::Error error) override;
  void OnOldEncryptionDetected(const UserContext& user_context) override;

  // KioskAppLauncher::Delegate:
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;
  bool IsShowingNetworkConfigScreen() const override;
  bool ShouldSkipAppInstallation() const override;
  void OnAppDataUpdated() override;
  void OnAppLaunched() override;
  void OnAppWindowCreated() override;

  // AppLaunchSplashScreenView::Delegate:
  KioskAppManagerBase::App GetAppData() override;
  void OnCancelAppLaunch() override;
  void OnDeletingSplashScreenView() override;

  // Accound id of the app we are currently running.
  AccountId account_id_;

  // LoginDisplayHost owns itself.
  LoginDisplayHost* const host_;
  // Owned by OobeUI.
  AppLaunchSplashScreenView* arc_kiosk_splash_screen_view_;
  // Not owning here.
  Profile* profile_ = nullptr;

  // Used to execute login operations.
  std::unique_ptr<KioskProfileLoader> kiosk_profile_loader_;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;
  bool launched_ = false;
  base::WeakPtrFactory<ArcKioskController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcKioskController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ARC_KIOSK_CONTROLLER_H_
