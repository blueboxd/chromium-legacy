// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace web_app {

namespace {

void OnShortcutInfoReceived(std::unique_ptr<ShortcutInfo> info) {
  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  internals::ScheduleCreatePlatformShortcuts(
      std::move(shortcut_data_dir), locations,
      ShortcutCreationReason::SHORTCUT_CREATION_BY_USER, std::move(info),
      base::DoNothing());
}

void UpdateFileHandlerRegistrationInOs(const AppId& app_id, Profile* profile) {
  // On Linux, file associations are managed through shortcuts in the app menu,
  // so after enabling or disabling file handling for an app its shortcuts
  // need to be recreated.
  AppShortcutManager& shortcut_manager =
      WebAppProviderBase::GetProviderBase(profile)->shortcut_manager();
  shortcut_manager.GetShortcutInfoForApp(
      app_id, base::BindOnce(&OnShortcutInfoReceived));
}

}  // namespace

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const std::set<std::string>& file_extensions,
                                const std::set<std::string>& mime_types) {
  UpdateFileHandlerRegistrationInOs(app_id, profile);
}

void UnregisterFileHandlersWithOs(const AppId& app_id, Profile* profile) {
  // If this was triggered as part of the uninstallation process, nothing more
  // is needed. Uninstalling already cleans up shortcuts (and thus, file
  // handlers).
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  if (!provider->registrar().IsInstalled(app_id))
    return;

  UpdateFileHandlerRegistrationInOs(app_id, profile);
}

}  // namespace web_app
