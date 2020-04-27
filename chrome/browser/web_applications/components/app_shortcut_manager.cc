// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_shortcut_manager.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_shortcut_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/components/app_shim_registry_mac.h"
#endif

namespace web_app {

AppShortcutManager::AppShortcutManager(Profile* profile) : profile_(profile) {}

AppShortcutManager::~AppShortcutManager() {
  for (auto& observer : observers_)
    observer.OnShortcutManagerDestroyed();
}

void AppShortcutManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
}

void AppShortcutManager::Start() {
  DCHECK(registrar_);
  app_registrar_observer_.Add(registrar_);

#if defined(OS_MACOSX)
  // Ensure that all installed apps are included in the AppShimRegistry when the
  // profile is loaded. This is redundant, because apps are registered when they
  // are installed. It is necessary, however, because app registration was added
  // long after app installation launched. This should be removed after shipping
  // for a few versions (whereupon it may be assumed that most applications have
  // been registered).
  std::vector<AppId> app_ids = registrar_->GetAppIds();
  for (const auto& app_id : app_ids) {
    AppShimRegistry::Get()->OnAppInstalledForProfile(app_id,
                                                     profile_->GetPath());
  }
#endif
}

void AppShortcutManager::Shutdown() {
  app_registrar_observer_.RemoveAll();
}

void AppShortcutManager::AddObserver(AppShortcutObserver* observer) {
  observers_.AddObserver(observer);
}

void AppShortcutManager::RemoveObserver(AppShortcutObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppShortcutManager::OnWebAppInstalled(const AppId& app_id) {
#if defined(OS_MACOSX)
  AppShimRegistry::Get()->OnAppInstalledForProfile(app_id, profile_->GetPath());
#endif
}

void AppShortcutManager::OnWebAppUninstalled(const AppId& app_id) {
  DeleteSharedAppShims(app_id);
}

void AppShortcutManager::OnWebAppProfileWillBeDeleted(const AppId& app_id) {
  DeleteSharedAppShims(app_id);
}

void AppShortcutManager::DeleteSharedAppShims(const AppId& app_id) {
#if defined(OS_MACOSX)
  bool delete_multi_profile_shortcuts =
      AppShimRegistry::Get()->OnAppUninstalledForProfile(app_id,
                                                         profile_->GetPath());
  if (delete_multi_profile_shortcuts) {
    web_app::internals::GetShortcutIOTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&web_app::internals::DeleteMultiProfileShortcutsForApp,
                       app_id));
  }
#endif
}

bool AppShortcutManager::CanCreateShortcuts() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void AppShortcutManager::SuppressShortcutsForTesting() {
  suppress_shortcuts_for_testing_ = true;
}

void AppShortcutManager::CreateShortcuts(const AppId& app_id,
                                         bool add_to_desktop,
                                         CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(CanCreateShortcuts());

  GetShortcutInfoForApp(
      app_id, base::BindOnce(
                  &AppShortcutManager::OnShortcutInfoRetrievedCreateShortcuts,
                  weak_ptr_factory_.GetWeakPtr(), add_to_desktop,
                  base::BindOnce(&AppShortcutManager::OnShortcutsCreated,
                                 weak_ptr_factory_.GetWeakPtr(), app_id,
                                 std::move(callback))));
}

void AppShortcutManager::OnShortcutsCreated(const AppId& app_id,
                                            CreateShortcutsCallback callback,
                                            bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    for (auto& observer : observers_)
      observer.OnShortcutsCreated(app_id);
  }
  std::move(callback).Run(success);
}

void AppShortcutManager::OnShortcutInfoRetrievedCreateShortcuts(
    bool add_to_desktop,
    CreateShortcutsCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  if (suppress_shortcuts_for_testing_) {
    std::move(callback).Run(true);
    return;
  }

  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.on_desktop = add_to_desktop;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  internals::ScheduleCreatePlatformShortcuts(
      std::move(shortcut_data_dir), locations, SHORTCUT_CREATION_BY_USER,
      std::move(info), std::move(callback));
}

}  // namespace web_app
