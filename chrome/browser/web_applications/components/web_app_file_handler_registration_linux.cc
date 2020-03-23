// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
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

void OnRegisterMimeTypes(bool registration_succeeded) {
  if (!registration_succeeded)
    LOG(ERROR) << "Registering MIME types failed.";
}

bool DoRegisterMimeTypes(base::FilePath filename, std::string file_contents) {
  DCHECK(!filename.empty() && !file_contents.empty());

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  base::FilePath temp_file_path(temp_dir.GetPath().Append(filename));

  int bytes_written = base::WriteFile(temp_file_path, file_contents.data(),
                                      file_contents.length());
  if (bytes_written != static_cast<int>(file_contents.length()))
    return false;

  std::vector<std::string> argv{"xdg-mime", "install", "--mode", "user",
                                temp_file_path.value()};

  int exit_code;
  shell_integration_linux::LaunchXdgUtility(argv, &exit_code);
  return exit_code == 0;
}

RegisterMimeTypesOnLinuxCallback& GetRegisterMimeTypesCallbackForTesting() {
  static base::NoDestructor<RegisterMimeTypesOnLinuxCallback> instance;
  return *instance;
}

}  // namespace

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers) {
  if (!file_handlers.empty()) {
    RegisterMimeTypesOnLinuxCallback callback =
        GetRegisterMimeTypesCallbackForTesting()
            ? std::move(GetRegisterMimeTypesCallbackForTesting())
            : base::BindOnce(&DoRegisterMimeTypes);
    RegisterMimeTypesOnLinux(app_id, profile, file_handlers,
                             std::move(callback));
  }

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

void RegisterMimeTypesOnLinux(const AppId& app_id,
                              Profile* profile,
                              const apps::FileHandlers& file_handlers,
                              RegisterMimeTypesOnLinuxCallback callback) {
  DCHECK(!app_id.empty() && !file_handlers.empty());

  base::FilePath filename =
      shell_integration_linux::GetMimeTypesRegistrationFilename(
          profile->GetPath(), app_id);
  std::string file_contents =
      shell_integration_linux::GetMimeTypesRegistrationFileContents(
          file_handlers);

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(filename),
                     std::move(file_contents)),
      base::BindOnce(&OnRegisterMimeTypes));
}

void SetRegisterMimeTypesOnLinuxCallbackForTesting(
    RegisterMimeTypesOnLinuxCallback callback) {
  GetRegisterMimeTypesCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
