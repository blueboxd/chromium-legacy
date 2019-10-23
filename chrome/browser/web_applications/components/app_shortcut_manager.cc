// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_shortcut_manager.h"

#include "base/callback.h"
#include "chrome/browser/web_applications/components/app_shortcut_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

void OnShortcutInfoRetrievedCreateShortcuts(
    bool add_to_desktop,
    CreateShortcutsCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.on_desktop = add_to_desktop;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  internals::ScheduleCreatePlatformShortcuts(
      std::move(shortcut_data_dir), locations, SHORTCUT_CREATION_BY_USER,
      std::move(info), std::move(callback));
}

}  // namespace

AppShortcutManager::AppShortcutManager(Profile* profile) : profile_(profile) {}

AppShortcutManager::~AppShortcutManager() = default;

void AppShortcutManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
}

void AppShortcutManager::Shutdown() {
  for (auto& observer : observers_)
    observer.OnShortcutManagerDestroyed();
}

void AppShortcutManager::AddObserver(AppShortcutObserver* observer) {
  observers_.AddObserver(observer);
}

void AppShortcutManager::RemoveObserver(AppShortcutObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AppShortcutManager::CanCreateShortcuts() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void AppShortcutManager::CreateShortcuts(const AppId& app_id,
                                         bool add_to_desktop,
                                         CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(CanCreateShortcuts());
  GetShortcutInfoForApp(
      app_id,
      base::BindOnce(&OnShortcutInfoRetrievedCreateShortcuts, add_to_desktop,
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

}  // namespace web_app
