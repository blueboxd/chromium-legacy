// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_

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

// An enum that lists the different System Apps that exist. Can be used to
// retrieve the App ID from the underlying Web App system.
enum class SystemAppType {
  SETTINGS,
  DISCOVER,
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

  // Constructs a SystemWebAppManager instance that uses
  // |pending_app_manager| to manage apps. |pending_app_manager| should outlive
  // this class.
  SystemWebAppManager(Profile* profile, PendingAppManager* pending_app_manager);
  virtual ~SystemWebAppManager();

  void Start();

  static bool IsEnabled();

  // The SystemWebAppManager is disabled in browser tests by default because it
  // pollutes the startup state. Call this to enable them for SystemWebApp
  // specific tests.
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
      base::flat_map<SystemAppType, GURL> system_app_urls);
  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  virtual const base::Version& CurrentVersion() const;

 private:
  void OnAppsSynchronized(PendingAppManager::SynchronizeResult result);
  bool NeedsUpdate() const;

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;

  UpdatePolicy update_policy_;

  base::flat_map<SystemAppType, GURL> system_app_urls_;

  PrefService* pref_service_;
  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
