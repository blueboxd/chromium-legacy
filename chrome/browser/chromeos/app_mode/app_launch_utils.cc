// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_launch_utils.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

namespace {

// The list of prefs that are reset on the start of each kiosk session.
const char* const kPrefsToReset[] = {"settings.accessibility",  // ChromeVox
                                     "settings.a11y", "ash.docked_magnifier"};

// This vector is used in tests when they want to replace |kPrefsToReset| with
// their own list.
std::vector<std::string>* test_prefs_to_reset = nullptr;

}  // namespace

// A simple manager for the app launch that starts the launch
// and deletes itself when the launch finishes. On launch failure,
// it exits the browser process.
class AppLaunchManager : public StartupAppLauncher::Delegate {
 public:
  AppLaunchManager(Profile* profile, const std::string& app_id)
      : startup_app_launcher_(
            new StartupAppLauncher(profile,
                                   app_id,
                                   false /* diagnostic_mode */,
                                   this)) {}

  void Start() { startup_app_launcher_->Initialize(); }

 private:
  ~AppLaunchManager() override {}

  void Cleanup() { delete this; }

  // StartupAppLauncher::Delegate overrides:
  void InitializeNetwork() override {
    // This is on crash-restart path and assumes network is online.
    // TODO(xiyuan): Remove the crash-restart path for kiosk or add proper
    // network configure handling.
    startup_app_launcher_->ContinueWithNetworkReady();
  }
  bool IsNetworkReady() override {
    // See comments above. Network is assumed to be online here.
    return true;
  }
  bool ShouldSkipAppInstallation() override {
    // Given that this delegate does not reliably report whether the network is
    // ready, avoid making app update checks - this might take a while if
    // network is not online. Also, during crash-restart, we should continue
    // with the same app version as the restored session.
    return true;
  }
  void OnInstallingApp() override {}
  void OnReadyToLaunch() override { startup_app_launcher_->LaunchApp(); }
  void OnLaunchSucceeded() override { Cleanup(); }
  void OnLaunchFailed(KioskAppLaunchError::Error error) override {
    KioskAppLaunchError::Save(error);
    chrome::AttemptUserExit();
    Cleanup();
  }
  bool IsShowingNetworkConfigScreen() override { return false; }

  std::unique_ptr<StartupAppLauncher> startup_app_launcher_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchManager);
};

void LaunchAppOrDie(Profile* profile, const std::string& app_id) {
  // AppLaunchManager manages its own lifetime.
  (new AppLaunchManager(profile, app_id))->Start();
}

void ResetEphemeralKioskPreferences(PrefService* prefs) {
  CHECK(prefs);
  CHECK(user_manager::UserManager::IsInitialized() &&
        user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());
  for (size_t pref_id = 0;
       pref_id < (test_prefs_to_reset ? test_prefs_to_reset->size()
                                      : base::size(kPrefsToReset));
       pref_id++) {
    const std::string branch_path = test_prefs_to_reset
                                        ? (*test_prefs_to_reset)[pref_id]
                                        : kPrefsToReset[pref_id];
    prefs->ClearPrefsWithPrefixSilently(branch_path);
  }
}

void SetEphemeralKioskPreferencesListForTesting(
    std::vector<std::string>* prefs) {
  test_prefs_to_reset = prefs;
}

}  // namespace chromeos
