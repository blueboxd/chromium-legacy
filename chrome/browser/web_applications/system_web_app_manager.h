// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "url/gurl.h"

namespace base {
class Version;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class Profile;

namespace web_app {

class WebAppUiManager;

// An enum that lists the different System Apps that exist. Can be used to
// retrieve the App ID from the underlying Web App system.
enum class SystemAppType {
  SETTINGS,
  DISCOVER,
  CAMERA,
};

// The configuration options for a System App.
struct SystemAppInfo {
  // The URL that the System App will be installed from.
  GURL install_url;

  // If specified, the app with AppId |migration_source| will have its data
  // migrated to this System App.
  AppId migration_source;
};

// Installs, uninstalls, and updates System Web Apps.
class SystemWebAppManager {
 public:
  // Policy for when the SystemWebAppManager will update apps/install new apps.
  enum class UpdatePolicy {
    // Update every system start.
    kAlwaysUpdate,
    // Update when the Chrome version number changes.
    kOnVersionChange,
  };

  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.System";

  // Returns whether the given app type is enabled.
  static bool IsAppEnabled(SystemAppType type);

  explicit SystemWebAppManager(Profile* profile);
  virtual ~SystemWebAppManager();

  void SetSubsystems(PendingAppManager* pending_app_manager,
                     AppRegistrar* registrar,
                     WebAppUiManager* ui_manager);

  void Start();

  static bool IsEnabled();

  // The SystemWebAppManager is disabled in browser tests by default because it
  // pollutes the startup state (several tests expect the Extensions state to be
  // clean).
  //
  // Call this to install apps for SystemWebApp specific tests, e.g if a test
  // needs to open OS Settings.
  //
  // This can also be called multiple times to simulate reinstallation from
  // system restart, e.g.
  void InstallSystemAppsForTesting();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the app id for the given System App |id|.
  base::Optional<AppId> GetAppIdForSystemApp(SystemAppType id) const;

  // Returns whether |app_id| points to an installed System App.
  bool IsSystemWebApp(const AppId& app_id) const;

  const base::OneShotEvent& on_apps_synchronized() const {
    return *on_apps_synchronized_;
  }

 protected:
  void SetSystemAppsForTesting(
      base::flat_map<SystemAppType, SystemAppInfo> system_apps);
  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  virtual const base::Version& CurrentVersion() const;

 private:
  void OnAppsSynchronized(std::set<SystemAppType> already_installed,
                          std::map<GURL, InstallResultCode> install_results,
                          std::map<GURL, bool> uninstall_results);
  bool NeedsUpdate() const;

  // TODO(calamity): Move migration into the install task once the install task
  // is able to distinguish between an update install and a fresh install.
  void MigrateSystemWebApps(std::set<SystemAppType> already_installed);

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;

  UpdatePolicy update_policy_;

  base::flat_map<SystemAppType, SystemAppInfo> system_app_infos_;

  PrefService* pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_ = nullptr;

  AppRegistrar* registrar_ = nullptr;

  WebAppUiManager* ui_manager_ = nullptr;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
