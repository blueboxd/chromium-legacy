// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

namespace web_app {

namespace {

// kAppUrl is a required string specifying a URL inside the scope of the web
// app that contains a link to the app manifest.
constexpr char kAppUrl[] = "app_url";

// kHideFromUser is an optional boolean which controls whether we add
// a shortcut to the relevant OS surface i.e. Application folder on macOS, Start
// Menu on Windows and Linux, and launcher on Chrome OS. Defaults to false if
// missing. If true, we also don't show the app in search or in app management
// on Chrome OS.
constexpr char kHideFromUser[] = "hide_from_user";

// kCreateShortcuts is an optional boolean which controls whether OS
// level shortcuts are created. On Chrome OS this controls whether the app is
// pinned to the shelf.
// The default value of kCreateShortcuts if false.
constexpr char kCreateShortcuts[] = "create_shortcuts";

// kFeatureName is an optional string parameter specifying a feature
// associated with this app. The feature must be present in
// |kExternalAppInstallFeatures| to be applicable.
// If specified:
//  - if the feature is enabled, the app will be installed
//  - if the feature is not enabled, the app will be removed.
constexpr char kFeatureName[] = "feature_name";

// kLaunchContainer is a required string which can be "window" or "tab"
// and controls what sort of container the web app is launched in.
constexpr char kLaunchContainer[] = "launch_container";
constexpr char kLaunchContainerTab[] = "tab";
constexpr char kLaunchContainerWindow[] = "window";

// kUninstallAndReplace is an optional array of strings which specifies App IDs
// which the app is replacing. This will transfer OS attributes (e.g the source
// app's shelf and app list positions on ChromeOS) and then uninstall the source
// app.
constexpr char kUninstallAndReplace[] = "uninstall_and_replace";

#if defined(OS_CHROMEOS)
// The sub-directory of the extensions directory in which to scan for external
// web apps (as opposed to external extensions or external ARC apps).
const base::FilePath::CharType kWebAppsSubDirectory[] =
    FILE_PATH_LITERAL("web_apps");
#endif

bool g_skip_startup_scan_for_testing_ = false;

base::Optional<ExternalInstallOptions> ParseConfig(
    base::FilePath file,
    const std::string& user_type,
    const base::Value& app_config) {
  if (app_config.type() != base::Value::Type::DICTIONARY) {
    LOG(ERROR) << file << " was not a dictionary as the top level";
    return base::nullopt;
  }

  if (!apps::UserTypeMatchesJsonUserType(
          user_type, /*app_id=*/file.MaybeAsASCII(), &app_config,
          /*default_user_types=*/nullptr)) {
    // Already logged.
    return base::nullopt;
  }

  const base::Value* value =
      app_config.FindKeyOfType(kFeatureName, base::Value::Type::STRING);
  if (value) {
    // TODO(crbug.com/1104696): Add metrics for whether the app was
    // enabled/disabled by the feature.
    const std::string& feature_name = value->GetString();
    VLOG(1) << file << " checking feature " << feature_name;
    if (!IsExternalAppInstallFeatureEnabled(feature_name)) {
      VLOG(1) << file << " feature not enabled";
      return base::nullopt;
    }
  }

  value = app_config.FindKeyOfType(kAppUrl, base::Value::Type::STRING);
  if (!value) {
    LOG(ERROR) << file << " had a missing " << kAppUrl;
    return base::nullopt;
  }
  GURL app_url(value->GetString());
  if (!app_url.is_valid()) {
    LOG(ERROR) << file << " had an invalid " << kAppUrl;
    return base::nullopt;
  }

  bool hide_from_user = false;
  value = app_config.FindKey(kHideFromUser);
  if (value) {
    if (!value->is_bool()) {
      LOG(ERROR) << file << " had an invalid " << kHideFromUser;
      return base::nullopt;
    }
    hide_from_user = value->GetBool();
  }

  bool create_shortcuts = false;
  value = app_config.FindKey(kCreateShortcuts);
  if (value) {
    if (!value->is_bool()) {
      LOG(ERROR) << file << " had an invalid " << kCreateShortcuts;
      return base::nullopt;
    }
    create_shortcuts = value->GetBool();
  }

  // It doesn't make sense to hide the app and also create shortcuts for it.
  DCHECK(!(hide_from_user && create_shortcuts));

  value = app_config.FindKeyOfType(kLaunchContainer, base::Value::Type::STRING);
  if (!value) {
    LOG(ERROR) << file << " had an invalid " << kLaunchContainer;
    return base::nullopt;
  }
  std::string launch_container_str = value->GetString();
  auto user_display_mode = DisplayMode::kBrowser;
  if (launch_container_str == kLaunchContainerTab) {
    user_display_mode = DisplayMode::kBrowser;
  } else if (launch_container_str == kLaunchContainerWindow) {
    user_display_mode = DisplayMode::kStandalone;
  } else {
    LOG(ERROR) << file << " had an invalid " << kLaunchContainer;
    return base::nullopt;
  }

  value = app_config.FindKey(kUninstallAndReplace);
  std::vector<AppId> uninstall_and_replace_ids;
  if (value) {
    if (!value->is_list()) {
      LOG(ERROR) << file << " had an invalid " << kUninstallAndReplace;
      return base::nullopt;
    }
    base::Value::ConstListView uninstall_and_replace_values = value->GetList();

    bool had_error = false;
    for (const auto& app_id_value : uninstall_and_replace_values) {
      if (!app_id_value.is_string()) {
        had_error = true;
        LOG(ERROR) << file << " had an invalid " << kUninstallAndReplace
                   << " entry";
        break;
      }
      uninstall_and_replace_ids.push_back(app_id_value.GetString());
    }
    if (had_error)
      return base::nullopt;
  }

  ExternalInstallOptions install_options(
      std::move(app_url), user_display_mode,
      ExternalInstallSource::kExternalDefault);
  install_options.add_to_applications_menu = !hide_from_user;
  install_options.add_to_search = !hide_from_user;
  install_options.add_to_management = !hide_from_user;
  install_options.add_to_desktop = create_shortcuts;
  install_options.add_to_quick_launch_bar = create_shortcuts;
  install_options.require_manifest = true;
  install_options.uninstall_and_replace = std::move(uninstall_and_replace_ids);

  return install_options;
}

std::vector<ExternalInstallOptions> ScanDir(const base::FilePath& dir,
                                            const std::string& user_type) {
  std::vector<ExternalInstallOptions> install_options_list;
  if (!base::FeatureList::IsEnabled(features::kDefaultWebAppInstallation))
    return install_options_list;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath::StringType extension(FILE_PATH_LITERAL(".json"));
  base::FileEnumerator json_files(dir,
                                  false,  // Recursive.
                                  base::FileEnumerator::FILES);

  for (base::FilePath file = json_files.Next(); !file.empty();
       file = json_files.Next()) {
    if (!file.MatchesExtension(extension)) {
      continue;
    }

    JSONFileValueDeserializer deserializer(file);
    std::string error_msg;
    std::unique_ptr<base::Value> app_config =
        deserializer.Deserialize(nullptr, &error_msg);
    if (!app_config) {
      LOG(ERROR) << file.value() << " was not valid JSON: " << error_msg;
      continue;
    }
    base::Optional<ExternalInstallOptions> install_options =
        ParseConfig(file, user_type, *app_config);
    if (install_options.has_value())
      install_options_list.push_back(std::move(*install_options));
  }

  return install_options_list;
}

base::FilePath DetermineScanDir(const Profile* profile) {
  base::FilePath dir;
#if defined(OS_CHROMEOS)
  // As of mid 2018, only Chrome OS has default/external web apps, and
  // chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX,
  // which includes OS_CHROMEOS.

  if (chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    // For manual testing, you can change s/STANDALONE/USER/, as writing to
    // "$HOME/.config/chromium/test-user/.config/chromium/External
    // Extensions/web_apps" does not require root ACLs, unlike
    // "/usr/share/chromium/extensions/web_apps".
    if (!base::PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                                &dir)) {
      LOG(ERROR) << "ScanForExternalWebApps: base::PathService::Get failed";
    } else {
      dir = dir.Append(kWebAppsSubDirectory);
    }
  }

#endif
  return dir;
}

void OnExternalWebAppsSynchronized(
    std::map<GURL, InstallResultCode> install_results,
    std::map<GURL, bool> uninstall_results) {
  RecordExternalAppInstallResultCode("Webapp.InstallResult.Default",
                                     install_results);
}

}  // namespace

ExternalWebAppManager::ExternalWebAppManager(Profile* profile)
    : profile_(profile) {}

ExternalWebAppManager::~ExternalWebAppManager() = default;

void ExternalWebAppManager::SetSubsystems(
    PendingAppManager* pending_app_manager) {
  pending_app_manager_ = pending_app_manager;
}

void ExternalWebAppManager::Start() {
  if (!g_skip_startup_scan_for_testing_) {
    ScanForExternalWebApps(
        base::BindOnce(&ExternalWebAppManager::OnScanForExternalWebApps,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

// static
std::vector<ExternalInstallOptions>
ExternalWebAppManager::ScanDirForExternalWebAppsForTesting(
    const base::FilePath& dir,
    Profile* profile) {
  return ScanDir(dir, apps::DetermineUserType(profile));
}

void ExternalWebAppManager::ScanForExternalWebApps(ScanCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::FilePath dir = DetermineScanDir(profile_);
  if (dir.empty()) {
    std::move(callback).Run(std::vector<ExternalInstallOptions>());
    return;
  }
  // Do a two-part callback dance, across different TaskRunners.
  //
  // 1. Schedule ScanDir to happen on a background thread, so that we don't
  // block the UI thread. When that's done,
  // base::PostTaskAndReplyWithResult will bounce us back to the originating
  // thread (the UI thread).
  //
  // 2. In |callback|, forward the vector of ExternalInstallOptions on to the
  // pending_app_manager_, which can only be called on the UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ScanDir, dir, apps::DetermineUserType(profile_)),
      std::move(callback));
}

void ExternalWebAppManager::SkipStartupScanForTesting() {
  g_skip_startup_scan_for_testing_ = true;
}

void ExternalWebAppManager::SynchronizeAppsForTesting(
    std::vector<std::string> app_configs,
    PendingAppManager::SynchronizeCallback callback) {
  std::vector<ExternalInstallOptions> install_options_list;
  for (const std::string& app_config_string : app_configs) {
    base::Optional<base::Value> app_config =
        base::JSONReader::Read(app_config_string);
    DCHECK(app_config);

    base::Optional<ExternalInstallOptions> install_options =
        ParseConfig(base::FilePath().AppendASCII("test"),
                    apps::DetermineUserType(profile_), *app_config);
    DCHECK(install_options);

    install_options_list.push_back(std::move(*install_options));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalDefault,
      std::move(callback));
}

void ExternalWebAppManager::OnScanForExternalWebApps(
    std::vector<ExternalInstallOptions> desired_apps_install_options) {
  DCHECK(pending_app_manager_);
  pending_app_manager_->SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalDefault,
      base::BindOnce(&OnExternalWebAppsSynchronized));
}

}  //  namespace web_app
