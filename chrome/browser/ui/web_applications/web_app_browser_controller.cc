// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace web_app {

WebAppBrowserController::WebAppBrowserController(Browser* browser)
    : AppBrowserController(browser),
      registrar_(WebAppProvider::Get(browser->profile())->registrar()),
      app_id_(GetAppIdFromApplicationName(browser->app_name())) {}

WebAppBrowserController::~WebAppBrowserController() = default;

base::Optional<AppId> WebAppBrowserController::GetAppId() const {
  return app_id_;
}

bool WebAppBrowserController::CreatedForInstalledPwa() const {
  return true;
}

bool WebAppBrowserController::IsHostedApp() const {
  return true;
}

bool WebAppBrowserController::ShouldShowCustomTabBar() const {
  // TODO(https://crbug.com/966290): Complete implementation.
  return false;
}

bool WebAppBrowserController::HasTitlebarToolbar() const {
  return true;
}

gfx::ImageSkia WebAppBrowserController::GetWindowAppIcon() const {
  // TODO(https://crbug.com/966290): Complete implementation.
  return gfx::ImageSkia();
}

gfx::ImageSkia WebAppBrowserController::GetWindowIcon() const {
  // TODO(https://crbug.com/966290): Complete implementation.
  return gfx::ImageSkia();
}

base::Optional<SkColor> WebAppBrowserController::GetThemeColor() const {
  base::Optional<SkColor> web_theme_color =
      AppBrowserController::GetThemeColor();
  if (web_theme_color)
    return web_theme_color;

  return registrar_.GetAppThemeColor(app_id_);
}

GURL WebAppBrowserController::GetAppLaunchURL() const {
  return registrar_.GetAppLaunchURL(app_id_);
}

bool WebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  base::Optional<GURL> app_scope = registrar_.GetAppScope(app_id_);
  if (!app_scope)
    return false;

  // https://w3c.github.io/manifest/#navigation-scope
  // If url is same origin as scope and url path starts with scope path, return
  // true. Otherwise, return false.
  if (app_scope->GetOrigin() != url.GetOrigin())
    return false;

  std::string scope_path = app_scope->path();
  std::string url_path = url.path();
  return scope_path.size() <= url_path.size() &&
         scope_path == url_path.substr(0, scope_path.size());
}

std::string WebAppBrowserController::GetAppShortName() const {
  return registrar_.GetAppShortName(app_id_);
}

base::string16 WebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppLaunchURL());
}

bool WebAppBrowserController::CanUninstall() const {
  return WebAppUiManagerImpl::Get(browser()->profile())
      ->dialog_manager()
      .CanUninstallWebApp(app_id_);
}

void WebAppBrowserController::Uninstall() {
  WebAppUiManagerImpl::Get(browser()->profile())
      ->dialog_manager()
      .UninstallWebApp(app_id_, WebAppDialogManager::UninstallSource::kAppMenu,
                       browser()->window(), base::DoNothing());
}

bool WebAppBrowserController::IsInstalled() const {
  return registrar_.IsInstalled(app_id_);
}

}  // namespace web_app
