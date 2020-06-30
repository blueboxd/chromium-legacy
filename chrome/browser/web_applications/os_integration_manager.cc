// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration_manager.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"

namespace web_app {

OsIntegrationManager::OsIntegrationManager() = default;

OsIntegrationManager::~OsIntegrationManager() = default;

void OsIntegrationManager::SetSubsystems(
    AppShortcutManager* shortcut_manager,
    FileHandlerManager* file_handler_manager) {
  shortcut_manager_ = shortcut_manager;
  file_handler_manager_ = file_handler_manager;
}

void OsIntegrationManager::InstallOsHooks(const AppId& app_id,
                                          InstallOsHooksCallback callback) {
  DCHECK(shortcut_manager_);

  if (shortcut_manager_->CanCreateShortcuts()) {
    shortcut_manager_->CreateShortcuts(
        app_id, /*add_to_desktop=*/true,
        base::BindOnce(&OsIntegrationManager::OnShortcutsCreated,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(callback)));
  }
}

void OsIntegrationManager::OnShortcutsCreated(const AppId& app_id,
                                              InstallOsHooksCallback callback,
                                              bool shortcuts_created) {
  DCHECK(file_handler_manager_);

  // TODO(crbug.com/1087219): callback should be run after all hooks are
  // deployed, need to refactor filehandler to allow this.
  file_handler_manager_->EnableAndRegisterOsFileHandlers(app_id);
  shortcut_manager_->ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      app_id, base::BindOnce(&OsIntegrationManager::OnShortcutsMenuRegistered,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), shortcuts_created));
}

void OsIntegrationManager::OnShortcutsMenuRegistered(
    InstallOsHooksCallback callback,
    bool shortcuts_created,
    bool shortcuts_menu_registered) {
  std::move(callback).Run(shortcuts_created);
}

}  // namespace web_app
