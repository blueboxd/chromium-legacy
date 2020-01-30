// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

class ChromeLauncherController;

namespace arc {
class ArcAppShortcutsMenuBuilder;
}

namespace extensions {
class ContextMenuMatcher;
}

class AppServiceShelfContextMenu : public ShelfContextMenu {
 public:
  AppServiceShelfContextMenu(ChromeLauncherController* controller,
                             const ash::ShelfItem* item,
                             int64_t display_id);
  ~AppServiceShelfContextMenu() override;

  AppServiceShelfContextMenu(const AppServiceShelfContextMenu&) = delete;
  AppServiceShelfContextMenu& operator=(const AppServiceShelfContextMenu&) =
      delete;

  // ShelfContextMenu:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::mojom::MenuItemsPtr menu_items);

  // Build additional extension app menu items.
  void BuildExtensionAppShortcutsMenu(ui::SimpleMenuModel* menu_model);

  // Build additional ARC app shortcuts menu items.
  // TODO(crbug.com/1038487): consider merging into AppService.
  void BuildArcAppShortcutsMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                                GetMenuModelCallback callback);

  void ShowAppInfo();

  // Helpers to set the launch type.
  void SetLaunchType(int command_id);

  // Helpers to set the launch type for the extension item.
  void SetExtensionLaunchType(int command_id);

  // Helpers to get the launch type for the extension item.
  extensions::LaunchType GetExtensionLaunchType() const;

  bool ShouldAddPinMenu();

  apps::mojom::AppType app_type_;

  // The SimpleMenuModel used to hold the submenu items.
  std::unique_ptr<ui::SimpleMenuModel> submenu_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  std::unique_ptr<arc::ArcAppShortcutsMenuBuilder> arc_shortcuts_menu_builder_;

  base::WeakPtrFactory<AppServiceShelfContextMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_SHELF_CONTEXT_MENU_H_
