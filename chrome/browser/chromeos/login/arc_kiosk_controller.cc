// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/arc_kiosk_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

// ARC Kiosk splash screen minimum show time.
constexpr base::TimeDelta kArcKioskSplashScreenMinTime =
    base::TimeDelta::FromSeconds(10);

ArcKioskController::ArcKioskController(LoginDisplayHost* host, OobeUI* oobe_ui)
    : host_(host),
      arc_kiosk_splash_screen_view_(
          oobe_ui->GetView<AppLaunchSplashScreenHandler>()) {}

ArcKioskController::~ArcKioskController() {
  if (arc_kiosk_splash_screen_view_)
    arc_kiosk_splash_screen_view_->SetDelegate(nullptr);
}

void ArcKioskController::StartArcKiosk(const AccountId& account_id) {
  DVLOG(1) << "Starting ARC Kiosk for account: " << account_id.GetUserEmail();

  account_id_ = account_id;

  host_->GetLoginDisplay()->SetUIEnabled(true);

  arc_kiosk_splash_screen_view_->SetDelegate(this);
  arc_kiosk_splash_screen_view_->Show();
  splash_wait_timer_.Start(
      FROM_HERE, kArcKioskSplashScreenMinTime,
      base::BindOnce(&ArcKioskController::CloseSplashScreen,
                     weak_ptr_factory_.GetWeakPtr()));

  kiosk_profile_loader_ = std::make_unique<KioskProfileLoader>(
      account_id, KioskAppType::ARC_APP, false, this);
  kiosk_profile_loader_->Start();
}

void ArcKioskController::OnCancelAppLaunch() {
  if (ArcKioskAppManager::Get()->GetDisableBailoutShortcut())
    return;

  KioskAppLaunchError::Save(KioskAppLaunchError::USER_CANCEL);
  CleanUp();
  chrome::AttemptUserExit();
}

void ArcKioskController::OnDeletingSplashScreenView() {
  arc_kiosk_splash_screen_view_ = nullptr;
}

void ArcKioskController::CleanUp() {
  splash_wait_timer_.Stop();
  // Delegate is registered only when |profile_| is set.
  if (profile_)
    ArcKioskAppService::Get(profile_)->SetDelegate(nullptr);
}

void ArcKioskController::CloseSplashScreen() {
  if (!launched_)
    return;
  CleanUp();
  host_->Finalize(base::OnceClosure());
  session_manager::SessionManager::Get()->SessionStarted();
}

void ArcKioskController::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting app launch.";
  profile_ = profile;
  // This object could be deleted any time after successfully reporting
  // a profile load, so invalidate the delegate now.
  ArcKioskAppService::Get(profile_)->SetDelegate(this);

  // This is needed to trigger input method extensions being loaded.
  profile->InitChromeOSPreferences();

  // Reset virtual keyboard to use IME engines in app profile early.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();

  if (arc_kiosk_splash_screen_view_) {
    // In ARC kiosk mode, installing means waiting for app be registered.
    arc_kiosk_splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::
            APP_LAUNCH_STATE_INSTALLING_APPLICATION);
  }
}

void ArcKioskController::OnProfileLoadFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "ARC Kiosk launch failed. Will now shut down, error=" << error;
  CleanUp();
  chrome::AttemptUserExit();
}

void ArcKioskController::OnOldEncryptionDetected(
    const UserContext& user_context) {
  host_->StartWizard(EncryptionMigrationScreenView::kScreenId);
  EncryptionMigrationScreen* migration_screen =
      static_cast<EncryptionMigrationScreen*>(
          host_->GetWizardController()->current_screen());
  DCHECK(migration_screen);
  migration_screen->SetUserContext(user_context);
  migration_screen->SetupInitialView();
}

void ArcKioskController::OnAppDataUpdated() {
  // Invokes Show() to update the app title and icon.
  arc_kiosk_splash_screen_view_->Show();
}

void ArcKioskController::OnAppLaunched() {
  DVLOG(1) << "ARC Kiosk launch succeeded, wait for app window.";

  if (arc_kiosk_splash_screen_view_) {
    arc_kiosk_splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::
            APP_LAUNCH_STATE_WAITING_APP_WINDOW);
    // Invokes Show() to update the app title and icon.
    arc_kiosk_splash_screen_view_->Show();
  }
}

void ArcKioskController::OnAppWindowCreated() {
  DVLOG(1) << "App window created, closing splash screen.";
  launched_ = true;
  // If timer is running, do not remove splash screen for a few
  // more seconds to give the user ability to exit ARC kiosk.
  if (splash_wait_timer_.IsRunning())
    return;
  CloseSplashScreen();
}

KioskAppManagerBase::App ArcKioskController::GetAppData() {
  DCHECK(account_id_.is_valid());
  const ArcKioskAppData* arc_app =
      ArcKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(arc_app);
  KioskAppManagerBase::App app(*arc_app);
  return app;
}

// TODO(crbug.com/1015383): Add network handling logic for arc kiosk.
void ArcKioskController::InitializeNetwork() {}

bool ArcKioskController::IsNetworkReady() const {
  return true;
}

bool ArcKioskController::IsShowingNetworkConfigScreen() const {
  return false;
}

bool ArcKioskController::ShouldSkipAppInstallation() const {
  return false;
}

}  // namespace chromeos
