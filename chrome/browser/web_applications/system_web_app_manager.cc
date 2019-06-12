// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"

namespace web_app {

namespace {

base::flat_map<SystemAppType, GURL> CreateSystemWebApps() {
  base::flat_map<SystemAppType, GURL> urls;
// TODO(calamity): Split this into per-platform functions.
#if defined(OS_CHROMEOS)
  urls[SystemAppType::DISCOVER] = GURL(chrome::kChromeUIDiscoverURL);
  constexpr char kChromeSettingsPWAURL[] = "chrome://settings/pwa.html";
  urls[SystemAppType::SETTINGS] = GURL(kChromeSettingsPWAURL);
#endif  // OS_CHROMEOS

  return urls;
}

InstallOptions CreateInstallOptionsForSystemApp(const GURL& url) {
  DCHECK_EQ(content::kChromeUIScheme, url.scheme());

  web_app::InstallOptions install_options(url, LaunchContainer::kWindow,
                                          InstallSource::kSystemInstalled);
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.bypass_service_worker_check = true;
  install_options.always_update = true;
  return install_options;
}

}  // namespace

SystemWebAppManager::SystemWebAppManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : on_apps_synchronized_(new base::OneShotEvent()),
      pref_service_(profile->GetPrefs()),
      pending_app_manager_(pending_app_manager),
      weak_ptr_factory_(this) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    // Always update in tests, and return early to avoid populating with real
    // system apps.
    update_policy_ = UpdatePolicy::kAlwaysUpdate;
    return;
  }

#if defined(OFFICIAL_BUILD)
  // Official builds should trigger updates whenever the version number changes.
  update_policy_ = UpdatePolicy::kOnVersionChange;
#else
  // Dev builds should update every launch.
  update_policy_ = UpdatePolicy::kAlwaysUpdate;
#endif
  system_app_urls_ = CreateSystemWebApps();
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::Start() {
  // Clear the last update pref here to force uninstall, and to ensure that when
  // the flag is enabled again, an update is triggered.
  if (!IsEnabled())
    pref_service_->ClearPref(prefs::kSystemWebAppLastUpdateVersion);

  if (!NeedsUpdate())
    return;

  std::vector<InstallOptions> install_options_list;
  if (IsEnabled()) {
    // Skipping this will uninstall all System Apps currently installed.
    for (const auto& app : system_app_urls_) {
      install_options_list.push_back(
          CreateInstallOptionsForSystemApp(app.second));
    }
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), InstallSource::kSystemInstalled,
      base::BindOnce(&SystemWebAppManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemWebAppManager::InstallSystemAppsForTesting() {
  on_apps_synchronized_.reset(new base::OneShotEvent());
  system_app_urls_ = CreateSystemWebApps();
  Start();

  // Wait for the System Web Apps to install.
  base::RunLoop run_loop;
  on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

base::Optional<AppId> SystemWebAppManager::GetAppIdForSystemApp(
    SystemAppType id) const {
  auto app_url_it = system_app_urls_.find(id);

  if (app_url_it == system_app_urls_.end())
    return base::Optional<AppId>();

  return pending_app_manager_->LookupAppId(app_url_it->second);
}

bool SystemWebAppManager::IsSystemWebApp(const AppId& app_id) const {
  return pending_app_manager_->HasAppIdWithInstallSource(
      app_id, InstallSource::kSystemInstalled);
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, GURL> system_app_urls) {
  system_app_urls_ = std::move(system_app_urls);
}

void SystemWebAppManager::SetUpdatePolicyForTesting(UpdatePolicy policy) {
  update_policy_ = policy;
}

// static
bool SystemWebAppManager::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kSystemWebApps);
}

// static
void SystemWebAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSystemWebAppLastUpdateVersion, "");
}

const base::Version& SystemWebAppManager::CurrentVersion() const {
  return version_info::GetVersion();
}

void SystemWebAppManager::OnAppsSynchronized(
    PendingAppManager::SynchronizeResult result) {
  if (IsEnabled()) {
    pref_service_->SetString(prefs::kSystemWebAppLastUpdateVersion,
                             CurrentVersion().GetString());
  }

  if (!on_apps_synchronized_->is_signaled())
    on_apps_synchronized_->Signal();
}

bool SystemWebAppManager::NeedsUpdate() const {
  if (update_policy_ == UpdatePolicy::kAlwaysUpdate)
    return true;

  base::Version last_update_version(
      pref_service_->GetString(prefs::kSystemWebAppLastUpdateVersion));
  // This also updates if the version rolls back for some reason to ensure that
  // the System Web Apps are always in sync with the Chrome version.
  return !last_update_version.IsValid() ||
         last_update_version != CurrentVersion();
}

}  // namespace web_app
