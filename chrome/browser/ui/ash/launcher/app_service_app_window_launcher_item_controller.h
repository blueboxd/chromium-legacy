// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_

#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

// Shelf item delegate for extension app windows.
class AppServiceAppWindowLauncherItemController
    : public AppWindowLauncherItemController {
 public:
  explicit AppServiceAppWindowLauncherItemController(
      const ash::ShelfID& shelf_id);

  ~AppServiceAppWindowLauncherItemController() override;

  AppServiceAppWindowLauncherItemController(
      const AppServiceAppWindowLauncherItemController&) = delete;
  AppServiceAppWindowLauncherItemController& operator=(
      const AppServiceAppWindowLauncherItemController&) = delete;

  // aura::WindowObserver overrides:
  void OnWindowTitleChanged(aura::Window* window) override;

  // AppWindowLauncherItemController:
  AppMenuItems GetAppMenuItems(int event_flags) override;

 private:
  bool IsChromeApp();
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_WINDOW_LAUNCHER_ITEM_CONTROLLER_H_
