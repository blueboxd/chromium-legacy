// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"

namespace {

const extensions::Extension* GetExtensionForTab(Profile* profile,
                                                content::WebContents* tab) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service || !extension_service->extensions_enabled())
    return nullptr;

  // Note: It is possible to come here after a tab got removed form the browser
  // before it gets destroyed, in which case there is no browser.
  Browser* browser = chrome::FindBrowserWithWebContents(tab);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  // Use the Browser's app name to determine the extension for app windows and
  // use the tab's url for app tabs.
  if (browser && browser->deprecated_is_app()) {
    return registry->GetExtensionById(
        web_app::GetAppIdFromApplicationName(browser->app_name()),
        extensions::ExtensionRegistry::EVERYTHING);
  }

  const GURL url = tab->GetURL();
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension = extensions.GetAppByURL(url);
  if (extension && !extensions::LaunchesInWindow(profile, extension))
    return extension;

  // Bookmark app windows should match their launch url extension despite
  // their web extents.
  for (const auto& i : extensions) {
    if (i.get()->from_bookmark() &&
        extensions::IsInNavigationScopeForLaunchUrl(
            extensions::AppLaunchInfo::GetLaunchWebURL(i.get()), url) &&
        !extensions::LaunchesInWindow(profile, i.get())) {
      return i.get();
    }
  }
  return nullptr;
}

const extensions::Extension* GetExtensionByID(Profile* profile,
                                              const std::string& id) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      id, extensions::ExtensionRegistry::EVERYTHING);
}

}  // namespace

LauncherControllerHelper::LauncherControllerHelper(Profile* profile)
    : profile_(profile) {}

LauncherControllerHelper::~LauncherControllerHelper() {}

// static
base::string16 LauncherControllerHelper::GetAppTitle(
    Profile* profile,
    const std::string& app_id) {
  if (app_id.empty())
    return base::string16();

  // Get the title if the app is an ARC app. ARC shortcuts could call this
  // function when it's created, so AppService can't be used for ARC shortcuts,
  // because AppService is async.
  if (arc::IsArcItem(profile, app_id)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        ArcAppListPrefs::Get(profile)->GetApp(
            arc::ArcAppShelfId::FromString(app_id).app_id());
    DCHECK(app_info.get());
    return base::UTF8ToUTF16(app_info->name);
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (!proxy)
    return base::string16();

  std::string name;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&name](const apps::AppUpdate& update) { name = update.Name(); });
  return base::UTF8ToUTF16(name);
}

std::string LauncherControllerHelper::GetAppID(content::WebContents* tab) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    const std::vector<Profile*> profile_list =
        profile_manager->GetLoadedProfiles();
    if (!profile_list.empty()) {
      for (auto* i : profile_list) {
        const extensions::Extension* extension = GetExtensionForTab(i, tab);
        if (extension)
          return extension->id();
      }
      return std::string();
    }
  }
  // If there is no profile manager we only use the known profile.
  const extensions::Extension* extension = GetExtensionForTab(profile_, tab);
  return extension ? extension->id() : std::string();
}

bool LauncherControllerHelper::IsValidIDForCurrentUser(
    const std::string& app_id) const {
  if (IsValidIDForArcApp(app_id)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(features::kAppServiceShelf)) {
    return IsValidIDFromAppService(app_id);
  }

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_);
  if (registry_service && registry_service->IsCrostiniShelfAppId(app_id)) {
    return crostini::CrostiniFeatures::Get()->IsUIAllowed(profile_) &&
           registry_service->GetRegistration(app_id).has_value();
  }

  if (app_list::IsInternalApp(app_id)) {
    return true;
  }

  if (!GetExtensionByID(profile_, app_id)) {
    return false;
  }

  return true;
}

void LauncherControllerHelper::LaunchApp(const ash::ShelfID& id,
                                         ash::ShelfLaunchSource source,
                                         int event_flags,
                                         int64_t display_id) {
  // Handle recording app launch source from the Shelf in Demo Mode.
  if (source == ash::ShelfLaunchSource::LAUNCH_FROM_SHELF) {
    chromeos::DemoSession::RecordAppLaunchSourceIfInDemoMode(
        chromeos::DemoSession::AppLaunchSource::kShelf);
  }

  const std::string& app_id = id.app_id;
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  proxy->Launch(app_id, event_flags, apps::mojom::LaunchSource::kFromShelf,
                display_id);
  return;
}

ArcAppListPrefs* LauncherControllerHelper::GetArcAppListPrefs() const {
  return ArcAppListPrefs::Get(profile_);
}

bool LauncherControllerHelper::IsValidIDForArcApp(
    const std::string& app_id) const {
  const ArcAppListPrefs* arc_prefs = GetArcAppListPrefs();
  if (arc_prefs && arc_prefs->IsRegistered(app_id)) {
    return true;
  }

  if (app_id == arc::kPlayStoreAppId) {
    if (!arc::IsArcAllowedForProfile(profile()) ||
        !arc::IsPlayStoreAvailable()) {
      return false;
    }
    const arc::ArcSessionManager* arc_session_manager =
        arc::ArcSessionManager::Get();
    DCHECK(arc_session_manager);
    if (!arc_session_manager->IsAllowed()) {
      return false;
    }
    if (!arc::IsArcPlayStoreEnabledForProfile(profile()) &&
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile())) {
      return false;
    }
    return true;
  }

  return false;
}

bool LauncherControllerHelper::IsValidIDFromAppService(
    const std::string& app_id) const {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (!proxy) {
    return false;
  }
  apps::mojom::AppType app_type = proxy->AppRegistryCache().GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kUnknown ||
      app_type == apps::mojom::AppType::kArc) {
    return false;
  }

  return true;
}
