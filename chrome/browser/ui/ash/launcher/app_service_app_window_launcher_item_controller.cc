// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_app_window_launcher_item_controller.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

AppServiceAppWindowLauncherItemController::
    AppServiceAppWindowLauncherItemController(const ash::ShelfID& shelf_id)
    : AppWindowLauncherItemController(shelf_id) {}

AppServiceAppWindowLauncherItemController::
    ~AppServiceAppWindowLauncherItemController() {}

ash::ShelfItemDelegate::AppMenuItems
AppServiceAppWindowLauncherItemController::GetAppMenuItems(int event_flags) {
  if (!IsChromeApp())
    return GetAppMenuItems(event_flags);

  AppMenuItems items;
  extensions::AppWindowRegistry* const app_window_registry =
      extensions::AppWindowRegistry::Get(
          ChromeLauncherController::instance()->profile());

  for (const ui::BaseWindow* window : windows()) {
    extensions::AppWindow* const app_window =
        app_window_registry->GetAppWindowForNativeWindow(
            window->GetNativeWindow());
    DCHECK(app_window);

    // Use the app's web contents favicon, or the app window's icon.
    favicon::FaviconDriver* const favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(
            app_window->web_contents());
    DCHECK(favicon_driver);
    gfx::ImageSkia image = favicon_driver->GetFavicon().AsImageSkia();
    if (image.isNull()) {
      const gfx::ImageSkia* app_icon = nullptr;
      if (app_window->GetNativeWindow()) {
        app_icon = app_window->GetNativeWindow()->GetProperty(
            aura::client::kAppIconKey);
      }
      if (app_icon && !app_icon->isNull())
        image = *app_icon;
    }

    items.push_back({app_window->GetTitle(), image});
  }
  return items;
}

void AppServiceAppWindowLauncherItemController::OnWindowTitleChanged(
    aura::Window* window) {
  if (!IsChromeApp())
    return;

  // For Chrome apps,Use the window title (if set) to differentiate
  // show_in_shelf window shelf items instead of the default behavior of using
  // the app name.
  ui::BaseWindow* const base_window = GetAppWindow(window);

  extensions::AppWindowRegistry* const app_window_registry =
      extensions::AppWindowRegistry::Get(
          ChromeLauncherController::instance()->profile());
  extensions::AppWindow* const app_window =
      app_window_registry->GetAppWindowForNativeWindow(
          base_window->GetNativeWindow());

  // Use the window title (if set) to differentiate show_in_shelf window shelf
  // items instead of the default behavior of using the app name.
  if (app_window->show_in_shelf()) {
    const base::string16 title = window->GetTitle();
    if (!title.empty())
      ChromeLauncherController::instance()->SetItemTitle(shelf_id(), title);
  }
}

bool AppServiceAppWindowLauncherItemController::IsChromeApp() {
  Profile* const profile = ChromeLauncherController::instance()->profile();
  apps::AppServiceProxy* const proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  return proxy->AppRegistryCache().GetAppType(shelf_id().app_id) ==
         apps::mojom::AppType::kExtension;
}
