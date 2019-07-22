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
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // defined(OS_CHROMEOS)

namespace web_app {

namespace {

base::flat_map<SystemAppType, SystemAppInfo> CreateSystemWebApps() {
  base::flat_map<SystemAppType, SystemAppInfo> infos;
// TODO(calamity): Split this into per-platform functions.
#if defined(OS_CHROMEOS)
  if (SystemWebAppManager::IsAppEnabled(SystemAppType::DISCOVER))
    infos[SystemAppType::DISCOVER] = {GURL(chrome::kChromeUIDiscoverURL)};

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::CAMERA)) {
    constexpr char kCameraAppPWAURL[] = "chrome://camera/pwa.html";
    infos[SystemAppType::CAMERA] = {GURL(kCameraAppPWAURL),
                                    app_list::kInternalAppIdCamera};
  }

  if (base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)) {
    constexpr char kChromeSettingsPWAURL[] = "chrome://os-settings/pwa.html";
    infos[SystemAppType::SETTINGS] = {GURL(kChromeSettingsPWAURL),
                                      app_list::kInternalAppIdSettings};
  } else {
    constexpr char kChromeSettingsPWAURL[] = "chrome://settings/pwa.html";
    infos[SystemAppType::SETTINGS] = {GURL(kChromeSettingsPWAURL),
                                      app_list::kInternalAppIdSettings};
  }
#endif  // OS_CHROMEOS

  return infos;
}

ExternalInstallOptions CreateInstallOptionsForSystemApp(
    const SystemAppInfo& info,
    bool force_update) {
  DCHECK_EQ(content::kChromeUIScheme, info.install_url.scheme());

  web_app::ExternalInstallOptions install_options(
      info.install_url, LaunchContainer::kWindow,
      ExternalInstallSource::kSystemInstalled);
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.bypass_service_worker_check = true;
  install_options.force_reinstall = force_update;
  return install_options;
}

}  // namespace

// static
const char SystemWebAppManager::kInstallResultHistogramName[];

// static
bool SystemWebAppManager::IsAppEnabled(SystemAppType type) {
#if defined(OS_CHROMEOS)
  switch (type) {
    case SystemAppType::SETTINGS:
      return true;
      break;
    case SystemAppType::DISCOVER:
      return base::FeatureList::IsEnabled(chromeos::features::kDiscoverApp);
      break;
    case SystemAppType::CAMERA:
      return base::FeatureList::IsEnabled(
          chromeos::features::kCameraSystemWebApp);
      break;
  }
#else
  return false;
#endif  // OS_CHROMEOS
}
SystemWebAppManager::SystemWebAppManager(Profile* profile)
    : on_apps_synchronized_(new base::OneShotEvent()),
      pref_service_(profile->GetPrefs()) {
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
  system_app_infos_ = CreateSystemWebApps();
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::SetSubsystems(PendingAppManager* pending_app_manager,
                                        AppRegistrar* registrar,
                                        WebAppUiManager* ui_manager) {
  pending_app_manager_ = pending_app_manager;
  registrar_ = registrar;
  ui_manager_ = ui_manager;
}

void SystemWebAppManager::Start() {
  std::map<AppId, GURL> installed_apps = registrar_->GetExternallyInstalledApps(
      ExternalInstallSource::kSystemInstalled);

  std::set<SystemAppType> installed_app_types;
  for (const auto& type_and_info : system_app_infos_) {
    const GURL& install_url = type_and_info.second.install_url;
    if (std::find_if(installed_apps.begin(), installed_apps.end(),
                     [install_url](const std::pair<AppId, GURL> id_and_url) {
                       return id_and_url.second == install_url;
                     }) != installed_apps.end()) {
      installed_app_types.insert(type_and_info.first);
    }
  }

  std::vector<ExternalInstallOptions> install_options_list;
  if (IsEnabled()) {
    // Skipping this will uninstall all System Apps currently installed.
    for (const auto& app : system_app_infos_) {
      install_options_list.push_back(
          CreateInstallOptionsForSystemApp(app.second, NeedsUpdate()));
    }
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kSystemInstalled,
      base::BindOnce(&SystemWebAppManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr(), installed_app_types));
}

void SystemWebAppManager::InstallSystemAppsForTesting() {
  on_apps_synchronized_.reset(new base::OneShotEvent());
  system_app_infos_ = CreateSystemWebApps();
  Start();

  // Wait for the System Web Apps to install.
  base::RunLoop run_loop;
  on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

base::Optional<AppId> SystemWebAppManager::GetAppIdForSystemApp(
    SystemAppType id) const {
  auto app_url_it = system_app_infos_.find(id);

  if (app_url_it == system_app_infos_.end())
    return base::Optional<AppId>();

  return registrar_->LookupExternalAppId(app_url_it->second.install_url);
}

bool SystemWebAppManager::IsSystemWebApp(const AppId& app_id) const {
  return registrar_->HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kSystemInstalled);
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, SystemAppInfo> system_apps) {
  system_app_infos_ = std::move(system_apps);
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
    std::set<SystemAppType> already_installed,
    std::map<GURL, InstallResultCode> install_results,
    std::map<GURL, bool> uninstall_results) {
  if (IsEnabled()) {
    pref_service_->SetString(prefs::kSystemWebAppLastUpdateVersion,
                             CurrentVersion().GetString());
  }

  RecordExternalAppInstallResultCode(kInstallResultHistogramName,
                                     install_results);

  MigrateSystemWebApps(already_installed);

  // May be called more than once in tests.
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

void SystemWebAppManager::MigrateSystemWebApps(
    std::set<SystemAppType> already_installed) {
  DCHECK(ui_manager_);

  for (const auto& type_and_info : system_app_infos_) {
    // Migrate if a migration source is specified and the app has been newly
    // installed.
    if (!type_and_info.second.migration_source.empty() &&
        !base::Contains(already_installed, type_and_info.first)) {
      base::Optional<AppId> system_app_id =
          GetAppIdForSystemApp(type_and_info.first);
      // TODO(crbug.com/977466): Replace with a DCHECK once we understand why
      // this is happening.
      if (!system_app_id) {
        LOG(ERROR) << "System App Type "
                   << static_cast<int>(type_and_info.first)
                   << " could not be found when running migration.";
        continue;
      }
      ui_manager_->MigrateOSAttributes(type_and_info.second.migration_source,
                                       *system_app_id);
    }
  }
}

}  // namespace web_app
