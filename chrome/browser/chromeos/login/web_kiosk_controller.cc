// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/login/web_kiosk_controller.h>

#include "base/bind_helpers.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/network_service_instance.h"

namespace chromeos {

namespace {

// Web Kiosk splash screen minimum show time.
constexpr base::TimeDelta kWebKioskSplashScreenMinTime =
    base::TimeDelta::FromSeconds(10);

// Whether we should skip the wait for minimum screen show time.
bool g_skip_splash_wait_for_testing = false;
bool g_block_app_launch_for_testing = false;

// Time of waiting for the network to be ready to start installation. Can be
// changed in tests.
constexpr base::TimeDelta kWebKioskNetworkWaitTime =
    base::TimeDelta::FromSeconds(10);
base::TimeDelta g_network_wait_time = kWebKioskNetworkWaitTime;

}  // namespace

WebKioskController::WebKioskController(LoginDisplayHost* host, OobeUI* oobe_ui)
    : host_(host),
      web_kiosk_splash_screen_view_(
          oobe_ui->GetView<AppLaunchSplashScreenHandler>()) {}

WebKioskController::~WebKioskController() {
  if (web_kiosk_splash_screen_view_)
    web_kiosk_splash_screen_view_->SetDelegate(nullptr);
}

void WebKioskController::StartWebKiosk(const AccountId& account_id) {
  account_id_ = account_id;
  web_kiosk_splash_screen_view_->SetDelegate(this);
  web_kiosk_splash_screen_view_->Show();

  // When testing, do not start splash screen timer since we control it
  // manually. Also, do not do any actual cryptohome operations.
  if (testing_)
    return;

  splash_wait_timer_.Start(FROM_HERE, kWebKioskSplashScreenMinTime,
                           base::BindOnce(&WebKioskController::OnTimerFire,
                                          weak_ptr_factory_.GetWeakPtr()));

  kiosk_profile_loader_.reset(
      new KioskProfileLoader(account_id, KioskAppType::WEB_APP, false, this));
  kiosk_profile_loader_->Start();
}

KioskAppManagerBase::App WebKioskController::GetAppData() {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);

  auto data = KioskAppManagerBase::App(*app);
  data.url = app->install_url();
  return data;
}

void WebKioskController::OnTimerFire() {
  // Start launching now.
  if (app_state_ == AppState::INSTALLED) {
    LaunchApp();
  } else {
    launch_on_install_ = true;
  }
}

void WebKioskController::OnCancelAppLaunch() {
  if (WebKioskAppManager::Get()->GetDisableBailoutShortcut())
    return;

  KioskAppLaunchError::Save(KioskAppLaunchError::USER_CANCEL);
  CleanUp();
  chrome::AttemptUserExit();
}

void WebKioskController::OnNetworkConfigRequested() {
  network_ui_state_ = NetworkUIState::NEED_TO_SHOW;
  switch (app_state_) {
    case AppState::CREATING_PROFILE:
    case AppState::INIT_NETWORK:
    case AppState::INSTALLED:
      MaybeShowNetworkConfigureUI();
      break;
    case AppState::INSTALLING:
      // When requesting to show network configure UI, we should cancel current
      // installation and restart it as soon as the network is configured.
      // This is identical to what happens when we lose network connection
      // during installation.
      network_ui_state_ = NetworkUIState::NEED_TO_SHOW;
      OnNetworkStateChanged(/*online*/ false);
      break;
    case AppState::LAUNCHED:
      // We do nothing since the splash screen is soon to be destroyed.
      break;
  }
}

void WebKioskController::OnNetworkConfigFinished() {
  network_ui_state_ = NetworkUIState::NOT_SHOWING;
  OnNetworkStateChanged(/*online=*/true);
  if (app_state_ == AppState::INSTALLED) {
    LaunchApp();
  }
}

void WebKioskController::MaybeShowNetworkConfigureUI() {
  if (!web_kiosk_splash_screen_view_)
    return;

  if (app_state_ == AppState::CREATING_PROFILE) {
    web_kiosk_splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::
            APP_LAUNCH_STATE_SHOWING_NETWORK_CONFIGURE_UI);
    return;
  }
  ShowNetworkConfigureUI();
}

void WebKioskController::ShowNetworkConfigureUI() {
  // We should stop timers since they may fire during network
  // configure UI.
  splash_wait_timer_.Stop();
  network_wait_timer_.Stop();
  launch_on_install_ = true;
  network_ui_state_ = NetworkUIState::SHOWING;
  web_kiosk_splash_screen_view_->ShowNetworkConfigureUI();
}

void WebKioskController::OnNetworkStateChanged(bool online) {
  if (app_state_ == AppState::INIT_NETWORK) {
    if (online && network_ui_state_ == NetworkUIState::NOT_SHOWING) {
      network_wait_timer_.Stop();
      app_launcher_->ContinueWithNetworkReady();
    }
  }

  if (app_state_ == AppState::INSTALLING) {
    if (!online) {
      app_launcher_->RestartLauncher();
      ShowNetworkConfigureUI();
    }
  }
}

void WebKioskController::OnDeletingSplashScreenView() {
  web_kiosk_splash_screen_view_ = nullptr;
}

void WebKioskController::CleanUp() {
  splash_wait_timer_.Stop();
}

void WebKioskController::CloseSplashScreen() {
  CleanUp();
  // Can be null in tests.
  if (host_)
    host_->Finalize(base::OnceClosure());
}

void WebKioskController::InitializeNetwork() {
  if (!web_kiosk_splash_screen_view_)
    return;

  network_wait_timer_.Start(FROM_HERE, g_network_wait_time, this,
                            &WebKioskController::OnNetworkWaitTimedOut);

  web_kiosk_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_PREPARING_NETWORK);

  app_state_ = AppState::INIT_NETWORK;

  if (web_kiosk_splash_screen_view_->IsNetworkReady())
    OnNetworkStateChanged(true);
}

bool WebKioskController::IsNetworkReady() const {
  return web_kiosk_splash_screen_view_ &&
         web_kiosk_splash_screen_view_->IsNetworkReady();
}

bool WebKioskController::IsShowingNetworkConfigScreen() const {
  return network_ui_state_ == NetworkUIState::SHOWING;
}

bool WebKioskController::ShouldSkipAppInstallation() const {
  return false;
}

void WebKioskController::OnNetworkWaitTimedOut() {
  DCHECK(app_state_ ==
         AppState::INIT_NETWORK);  // Otherwise we should be installing the app.
  DCHECK(network_ui_state_ == NetworkUIState::NOT_SHOWING);

  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  content::GetNetworkConnectionTracker()->GetConnectionType(&connection_type,
                                                            base::DoNothing());
  SYSLOG(WARNING) << "OnNetworkWaitTimedout... connection = "
                  << connection_type;

  ShowNetworkConfigureUI();
}

void WebKioskController::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting app launch.";
  // This is needed to trigger input method extensions being loaded.
  profile->InitChromeOSPreferences();

  // Reset virtual keyboard to use IME engines in app profile early.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();

  // Make keyboard config sync with the |VirtualKeyboardFeatures| policy.
  ChromeKeyboardControllerClient::Get()->SetKeyboardConfigFromPref(true);

  // Can be not null in tests.
  if (!app_launcher_)
    app_launcher_.reset(new WebKioskAppLauncher(profile, this, account_id_));
  app_launcher_->Initialize();
  if (network_ui_state_ == NetworkUIState::NEED_TO_SHOW)
    ShowNetworkConfigureUI();
}

void WebKioskController::OnProfileLoadFailed(KioskAppLaunchError::Error error) {
  OnLaunchFailed(error);
}

void WebKioskController::OnOldEncryptionDetected(
    const UserContext& user_context) {
  NOTREACHED();
}

void WebKioskController::OnAppInstalling() {
  app_state_ = AppState::INSTALLING;
  if (!web_kiosk_splash_screen_view_)
    return;
  web_kiosk_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_INSTALLING_APPLICATION);
  web_kiosk_splash_screen_view_->Show();
}

void WebKioskController::OnAppPrepared() {
  app_state_ = AppState::INSTALLED;

  if (!web_kiosk_splash_screen_view_)
    return;

  if (network_ui_state_ != NetworkUIState::NOT_SHOWING)
    return;

  web_kiosk_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_WAITING_APP_WINDOW);
  web_kiosk_splash_screen_view_->Show();
  if (launch_on_install_ || g_skip_splash_wait_for_testing)
    LaunchApp();
}

void WebKioskController::OnAppInstallFailed() {
  // We end up here when WebKioskAppLauncher was not able to obtain metadata
  // for the app.
  // This can happen in some temporary states -- we are under captive portal, or
  // there is a third-party authorization which causes redirect to url that
  // differs from the install url. We should proceed with launch in such cases,
  // expecting this situation to not happen upon next launch.
  app_state_ = AppState::INSTALLED;

  SYSLOG(WARNING) << "Failed to obtain app data, trying to launch anyway..";

  if (!web_kiosk_splash_screen_view_)
    return;
  web_kiosk_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_WAITING_APP_WINDOW_INSTALL_FAILED);
  web_kiosk_splash_screen_view_->Show();
  if (launch_on_install_ || g_skip_splash_wait_for_testing)
    LaunchApp();
}

void WebKioskController::LaunchApp() {
  if (g_block_app_launch_for_testing)
    return;

  DCHECK(app_state_ == AppState::INSTALLED);
  // We need to change the session state so we are able to create browser
  // windows.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  app_launcher_->LaunchApp();
}

void WebKioskController::OnAppLaunched() {
  app_state_ = AppState::LAUNCHED;
  session_manager::SessionManager::Get()->SessionStarted();
  CloseSplashScreen();
}

void WebKioskController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  if (error == KioskAppLaunchError::UNABLE_TO_INSTALL) {
    OnAppInstallFailed();
    return;
  }

  // Reboot on the recoverable cryptohome errors.
  if (error == KioskAppLaunchError::CRYPTOHOMED_NOT_RUNNING ||
      error == KioskAppLaunchError::ALREADY_MOUNTED) {
    // Do not save the error because saved errors would stop app from launching
    // on the next run.
    chrome::AttemptRelaunch();
    return;
  }

  // Saves the error and ends the session to go back to login screen.
  KioskAppLaunchError::Save(error);
  CleanUp();
  chrome::AttemptUserExit();
}

WebKioskController::WebKioskController() : host_(nullptr) {}

// static
std::unique_ptr<WebKioskController> WebKioskController::CreateForTesting(
    AppLaunchSplashScreenView* view,
    std::unique_ptr<WebKioskAppLauncher> app_launcher) {
  std::unique_ptr<WebKioskController> controller(new WebKioskController());
  controller->web_kiosk_splash_screen_view_ = view;
  controller->app_launcher_ = std::move(app_launcher);
  controller->testing_ = true;
  return controller;
}

// static
std::unique_ptr<base::AutoReset<bool>>
WebKioskController::SkipSplashScreenWaitForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_skip_splash_wait_for_testing, true);
}

// static
std::unique_ptr<base::AutoReset<base::TimeDelta>>
WebKioskController::SetNetworkWaitForTesting(base::TimeDelta wait_time) {
  return std::make_unique<base::AutoReset<base::TimeDelta>>(
      &g_network_wait_time, wait_time);
}

// static
std::unique_ptr<base::AutoReset<bool>>
WebKioskController::BlockAppLaunchForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_block_app_launch_for_testing, true);
}

}  // namespace chromeos
