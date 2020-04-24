// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"

namespace web_app {

base::Optional<SystemAppType> GetSystemWebAppTypeForAppId(Profile* profile,
                                                          AppId app_id) {
  auto* provider = WebAppProvider::Get(profile);
  return provider ? provider->system_web_app_manager().GetSystemAppTypeForAppId(
                        app_id)
                  : base::Optional<SystemAppType>();
}

base::Optional<AppId> GetAppIdForSystemWebApp(Profile* profile,
                                              SystemAppType app_type) {
  auto* provider = WebAppProvider::Get(profile);
  return provider
             ? provider->system_web_app_manager().GetAppIdForSystemApp(app_type)
             : base::Optional<AppId>();
}

base::Optional<apps::AppLaunchParams> CreateSystemWebAppLaunchParams(
    Profile* profile,
    SystemAppType app_type) {
  base::Optional<AppId> app_id = GetAppIdForSystemWebApp(profile, app_type);
  // TODO(calamity): Decide whether to report app launch failure or CHECK fail.
  if (!app_id)
    return base::nullopt;

  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);

  DisplayMode display_mode =
      provider->registrar().GetAppEffectiveDisplayMode(app_id.value());

  // TODO(calamity): Plumb through better launch sources from callsites.
  apps::AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      app_id.value(), /*event_flags=*/0,
      apps::mojom::AppLaunchSource::kSourceChromeInternal,
      display::kInvalidDisplayId, /*fallback_container=*/
      ConvertDisplayModeToAppLaunchContainer(display_mode));

  return params;
}

Browser* LaunchSystemWebApp(Profile* profile,
                            SystemAppType app_type,
                            const GURL& url,
                            bool* did_create) {
  if (did_create)
    *did_create = false;

  base::Optional<apps::AppLaunchParams> params =
      CreateSystemWebAppLaunchParams(profile, app_type);
  if (!params)
    return nullptr;
  params->override_url = url;

  return LaunchSystemWebApp(profile, app_type, url, *params, did_create);
}

namespace {
base::FilePath GetLaunchDirectory(
    const std::vector<base::FilePath>& launch_files) {
  // |launch_dir| is the directory that contains all |launch_files|. If
  // there are no launch files, launch_dir is empty.
  base::FilePath launch_dir =
      launch_files.size() ? launch_files[0].DirName() : base::FilePath();

#if DCHECK_IS_ON()
  // Check |launch_files| all come from the same directory.
  if (!launch_dir.empty()) {
    for (auto path : launch_files) {
      DCHECK_EQ(launch_dir, path.DirName());
    }
  }
#endif

  return launch_dir;
}
}  // namespace

Browser* LaunchSystemWebApp(Profile* profile,
                            SystemAppType app_type,
                            const GURL& url,
                            const apps::AppLaunchParams& params,
                            bool* did_create) {
  auto* provider = WebAppProvider::Get(profile);
  if (!provider)
    return nullptr;

  DCHECK_EQ(params.app_id, *GetAppIdForSystemWebApp(profile, app_type));

  // Make sure we have a browser for app.  Always reuse an existing browser for
  // popups, otherwise check app type whether we should use a single window.
  // TODO(crbug.com/1060423): Allow apps to control whether popups are single.
  Browser* browser = nullptr;
  Browser::Type browser_type = Browser::TYPE_APP;
  if (params.disposition == WindowOpenDisposition::NEW_POPUP)
    browser_type = Browser::TYPE_APP_POPUP;
  if (browser_type == Browser::TYPE_APP_POPUP ||
      provider->system_web_app_manager().IsSingleWindow(app_type)) {
    browser = FindSystemWebAppBrowser(profile, app_type, browser_type);
  }

  // We create the app window if no existing browser found.
  if (did_create)
    *did_create = !browser;

  content::WebContents* web_contents = nullptr;

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    if (!browser)
      browser = CreateWebApplicationWindow(profile, params.app_id,
                                           params.disposition);

    // Navigate application window to application's |url| if necessary.
    web_contents = browser->tab_strip_model()->GetWebContentsAt(0);
    if (!web_contents || web_contents->GetURL() != url) {
      web_contents = NavigateWebApplicationWindow(
          browser, params.app_id, url, WindowOpenDisposition::CURRENT_TAB);
    }
  } else {
    if (!browser)
      browser = CreateApplicationWindow(profile, params, url);

    // Navigate application window to application's |url| if necessary.
    web_contents = browser->tab_strip_model()->GetWebContentsAt(0);
    if (!web_contents || web_contents->GetURL() != url) {
      web_contents = NavigateApplicationWindow(
          browser, params, url, WindowOpenDisposition::CURRENT_TAB);
    }
  }

  // Send launch files.
  if (provider->file_handler_manager().IsFileHandlingAPIAvailable(
          params.app_id)) {
    if (provider->system_web_app_manager().AppShouldReceiveLaunchDirectory(
            app_type)) {
      web_launch::WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
          web_contents, web_contents->GetURL(),
          GetLaunchDirectory(params.launch_files), params.launch_files);
    } else {
      web_launch::WebLaunchFilesHelper::SetLaunchPaths(
          web_contents, web_contents->GetURL(), params.launch_files);
    }
  }

  browser->window()->Show();
  return browser;
}

Browser* FindSystemWebAppBrowser(Profile* profile,
                                 SystemAppType app_type,
                                 Browser::Type browser_type) {
  // TODO(calamity): Determine whether, during startup, we need to wait for
  // app install and then provide a valid answer here.
  base::Optional<AppId> app_id = GetAppIdForSystemWebApp(profile, app_type);
  if (!app_id)
    return nullptr;

  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);

  if (!provider->registrar().IsInstalled(app_id.value()))
    return nullptr;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile || browser->type() != browser_type)
      continue;

    if (GetAppIdFromApplicationName(browser->app_name()) == app_id.value())
      return browser;
  }

  return nullptr;
}

bool IsSystemWebApp(Browser* browser) {
  DCHECK(browser);
  return browser->app_controller() &&
         browser->app_controller()->is_for_system_web_app();
}

gfx::Size GetSystemWebAppMinimumWindowSize(Browser* browser) {
  DCHECK(browser);
  if (!browser->app_controller())
    return gfx::Size();  // Not an app.

  auto* app_controller = browser->app_controller();
  if (!app_controller->HasAppId())
    return gfx::Size();

  auto* provider = WebAppProvider::Get(browser->profile());
  if (!provider)
    return gfx::Size();

  return provider->system_web_app_manager().GetMinimumWindowSize(
      app_controller->GetAppId());
}

}  // namespace web_app
