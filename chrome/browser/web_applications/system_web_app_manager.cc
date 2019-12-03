// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/chromeos/extensions/default_web_app_ids.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // defined(OS_CHROMEOS)

namespace web_app {

namespace {

base::flat_map<SystemAppType, SystemAppInfo> CreateSystemWebApps() {
  base::flat_map<SystemAppType, SystemAppInfo> infos;
// TODO(calamity): Split this into per-platform functions.
#if defined(OS_CHROMEOS)
  // SystemAppInfo's |name| field should be defined. These names are persisted
  // to logs and should not be renamed.
  // If new names are added, update tool/metrics/histograms/histograms.xml:
  // "SystemWebAppName"
  if (SystemWebAppManager::IsAppEnabled(SystemAppType::DISCOVER)) {
    infos.emplace(
        SystemAppType::DISCOVER,
        SystemAppInfo("Discover", GURL(chrome::kChromeUIDiscoverURL)));
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::CAMERA)) {
    infos.emplace(SystemAppType::CAMERA,
                  SystemAppInfo("Camera", GURL("chrome://camera/pwa.html")));
    infos.at(SystemAppType::CAMERA).uninstall_and_replace = {
        ash::kInternalAppIdCamera};
  }

  if (base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)) {
    infos.emplace(
        SystemAppType::SETTINGS,
        SystemAppInfo("OSSettings", GURL("chrome://os-settings/pwa.html")));
    infos.at(SystemAppType::SETTINGS).uninstall_and_replace = {
        chromeos::default_web_apps::kSettingsAppId,
        ash::kInternalAppIdSettings};
  } else {
    infos.emplace(
        SystemAppType::SETTINGS,
        SystemAppInfo("BrowserSettings", GURL("chrome://settings/pwa.html")));
    infos.at(SystemAppType::SETTINGS).uninstall_and_replace = {
        ash::kInternalAppIdSettings};
  }
  // Large enough to see the heading text "Settings" in the top-left.
  infos.at(SystemAppType::SETTINGS).minimum_window_size = {300, 100};

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::TERMINAL)) {
    infos.emplace(
        SystemAppType::TERMINAL,
        SystemAppInfo("Terminal", GURL("chrome://terminal/html/pwa.html")));
    infos.at(SystemAppType::TERMINAL).single_window = false;
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::HELP)) {
    infos.emplace(SystemAppType::HELP,
                  SystemAppInfo("Help", GURL("chrome://help-app/pwa.html")));
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::MEDIA)) {
    infos.emplace(SystemAppType::MEDIA,
                  SystemAppInfo("Media", GURL("chrome://media-app/pwa.html")));
  }
#endif  // OS_CHROMEOS

  return infos;
}

ExternalInstallOptions CreateInstallOptionsForSystemApp(
    const SystemAppInfo& info,
    bool force_update) {
  DCHECK_EQ(content::kChromeUIScheme, info.install_url.scheme());

  ExternalInstallOptions install_options(
      info.install_url, DisplayMode::kStandalone,
      ExternalInstallSource::kSystemInstalled);
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.bypass_service_worker_check = true;
  install_options.force_reinstall = force_update;
  install_options.uninstall_and_replace = info.uninstall_and_replace;
  return install_options;
}

}  // namespace

SystemAppInfo::SystemAppInfo(const std::string& name_for_logging,
                             const GURL& install_url)
    : name_for_logging(name_for_logging), install_url(install_url) {}

SystemAppInfo::SystemAppInfo(const SystemAppInfo& other) = default;

SystemAppInfo::~SystemAppInfo() = default;

// static
const char SystemWebAppManager::kInstallResultHistogramName[];
const char SystemWebAppManager::kInstallDurationHistogramName[];

// static
bool SystemWebAppManager::IsAppEnabled(SystemAppType type) {
#if defined(OS_CHROMEOS)
  switch (type) {
    case SystemAppType::SETTINGS:
      return true;
    case SystemAppType::DISCOVER:
      return base::FeatureList::IsEnabled(chromeos::features::kDiscoverApp);
    case SystemAppType::CAMERA:
      return base::FeatureList::IsEnabled(
          chromeos::features::kCameraSystemWebApp);
    case SystemAppType::TERMINAL:
      return base::FeatureList::IsEnabled(features::kTerminalSystemApp);
    case SystemAppType::MEDIA:
      return base::FeatureList::IsEnabled(chromeos::features::kMediaApp);
    case SystemAppType::HELP:
      return base::FeatureList::IsEnabled(chromeos::features::kHelpAppV2);
  }
#else
  return false;
#endif  // OS_CHROMEOS
}
SystemWebAppManager::SystemWebAppManager(Profile* profile)
    : on_apps_synchronized_(new base::OneShotEvent()),
      install_result_per_profile_histogram_name_(
          std::string(kInstallResultHistogramName) + ".Profiles." +
          GetProfileCategoryForLogging(profile)),
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

void SystemWebAppManager::Shutdown() {
  shutting_down_ = true;
}

void SystemWebAppManager::SetSubsystems(PendingAppManager* pending_app_manager,
                                        AppRegistrar* registrar,
                                        WebAppUiManager* ui_manager) {
  pending_app_manager_ = pending_app_manager;
  registrar_ = registrar;
  ui_manager_ = ui_manager;
}

void SystemWebAppManager::Start() {
  const base::TimeTicks install_start_time = base::TimeTicks::Now();

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
                     weak_ptr_factory_.GetWeakPtr(), install_start_time));
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

base::Optional<SystemAppType> SystemWebAppManager::GetSystemAppTypeForAppId(
    AppId app_id) const {
  auto it = app_id_to_app_type_.find(app_id);
  if (it == app_id_to_app_type_.end())
    return base::nullopt;

  return it->second;
}

bool SystemWebAppManager::IsSystemWebApp(const AppId& app_id) const {
  return registrar_->HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kSystemInstalled);
}

bool SystemWebAppManager::IsSingleWindow(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;

  return it->second.single_window;
}

gfx::Size SystemWebAppManager::GetMinimumWindowSize(const AppId& app_id) const {
  auto app_type_it = app_id_to_app_type_.find(app_id);
  if (app_type_it == app_id_to_app_type_.end())
    return gfx::Size();
  const SystemAppType& app_type = app_type_it->second;
  auto app_info_it = system_app_infos_.find(app_type);
  if (app_info_it == system_app_infos_.end())
    return gfx::Size();
  return app_info_it->second.minimum_window_size;
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
  registry->RegisterStringPref(prefs::kSystemWebAppLastInstalledLocale, "");
}

const base::Version& SystemWebAppManager::CurrentVersion() const {
  return version_info::GetVersion();
}

const std::string& SystemWebAppManager::CurrentLocale() const {
  return g_browser_process->GetApplicationLocale();
}

void SystemWebAppManager::RecordSystemWebAppInstallMetrics(
    const std::map<GURL, InstallResultCode>& install_results,
    const base::TimeDelta& install_duration) const {
  // Record the time spent to install system web apps.
  if (!shutting_down_) {
    base::UmaHistogramMediumTimes(kInstallDurationHistogramName,
                                  install_duration);
  }

  // Record aggregate result.
  for (const auto& url_and_result : install_results)
    base::UmaHistogramEnumeration(kInstallResultHistogramName,
                                  shutting_down_
                                      ? InstallResultCode::kFailedShuttingDown
                                      : url_and_result.second);

  // Record per-app result.
  for (const auto type_and_app_info : system_app_infos_) {
    const GURL& install_url = type_and_app_info.second.install_url;
    const auto url_and_result = install_results.find(install_url);
    if (url_and_result != install_results.cend()) {
      const std::string app_histogram_name =
          std::string(kInstallResultHistogramName) + ".Apps." +
          type_and_app_info.second.name_for_logging;
      base::UmaHistogramEnumeration(app_histogram_name,
                                    shutting_down_
                                        ? InstallResultCode::kFailedShuttingDown
                                        : url_and_result->second);
    }
  }

  // Record per-profile result.
  for (const auto url_and_result : install_results) {
    base::UmaHistogramEnumeration(install_result_per_profile_histogram_name_,
                                  shutting_down_
                                      ? InstallResultCode::kFailedShuttingDown
                                      : url_and_result.second);
  }
}

void SystemWebAppManager::OnAppsSynchronized(
    const base::TimeTicks& install_start_time,
    std::map<GURL, InstallResultCode> install_results,
    std::map<GURL, bool> uninstall_results) {
  const base::TimeDelta install_duration =
      install_start_time - base::TimeTicks::Now();

  if (IsEnabled()) {
    // TODO(qjw): Figure out where install_results come from, decide if
    // installation failures need to be handled
    pref_service_->SetString(prefs::kSystemWebAppLastUpdateVersion,
                             CurrentVersion().GetString());
    pref_service_->SetString(prefs::kSystemWebAppLastInstalledLocale,
                             CurrentLocale());
  }

  RecordSystemWebAppInstallMetrics(install_results, install_duration);

  // Build the map from installed app id to app type.
  for (const auto& it : system_app_infos_) {
    const SystemAppType& app_type = it.first;
    base::Optional<AppId> app_id =
        registrar_->LookupExternalAppId(it.second.install_url);
    if (app_id.has_value())
      app_id_to_app_type_[app_id.value()] = app_type;
  }

  // May be called more than once in tests.
  if (!on_apps_synchronized_->is_signaled())
    on_apps_synchronized_->Signal();
}

bool SystemWebAppManager::NeedsUpdate() const {
  if (update_policy_ == UpdatePolicy::kAlwaysUpdate)
    return true;

  base::Version last_update_version(
      pref_service_->GetString(prefs::kSystemWebAppLastUpdateVersion));

  const std::string& last_installed_locale(
      pref_service_->GetString(prefs::kSystemWebAppLastInstalledLocale));

  // If Chrome version rolls back for some reason, ensure System Web Apps are
  // always in sync with Chrome version.
  bool versionIsDifferent =
      !last_update_version.IsValid() || last_update_version != CurrentVersion();

  // If system language changes, ensure System Web Apps launcher localization
  // are in sync with current language.
  bool localeIsDifferent = last_installed_locale != CurrentLocale();

  return versionIsDifferent || localeIsDifferent;
}

}  // namespace web_app
