// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MANAGER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

class AppShortcutManager;
class FileHandlerManager;

// Callback made when InstallOsHooks has finished trying to deploy all
// needed OS hooks.
using InstallOsHooksCallback = base::OnceCallback<void(bool shortcuts_created)>;

// OsIntegrationManager is responsible of creating/updating/deleting
// all OS hooks during Web App lifecycle.
// It contains individual OS integration managers and takes
// care of inter-dependencies among them.
class OsIntegrationManager {
 public:
  OsIntegrationManager();
  ~OsIntegrationManager();

  void SetSubsystems(AppShortcutManager* shortcut_manager,
                     FileHandlerManager* file_handler_manager);

  void InstallOsHooks(const AppId& app_id, InstallOsHooksCallback callback);

 private:
  void OnShortcutsCreated(const AppId& app_id,
                          InstallOsHooksCallback callback,
                          bool shortcuts_created);

  void OnShortcutsMenuRegistered(InstallOsHooksCallback callback,
                                 bool shortcuts_created,
                                 bool shortcuts_menu_registered);

  AppShortcutManager* shortcut_manager_ = nullptr;
  FileHandlerManager* file_handler_manager_ = nullptr;

  base::WeakPtrFactory<OsIntegrationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MANAGER_H_
