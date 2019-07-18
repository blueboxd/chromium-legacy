// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace web_app {

namespace {

ui::WindowShowState DetermineWindowShowState() {
  if (chrome::IsRunningInForcedAppMode())
    return ui::SHOW_STATE_FULLSCREEN;

  return ui::SHOW_STATE_DEFAULT;
}

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id) {
  std::string app_name = GenerateApplicationNameFromAppId(app_id);
  gfx::Rect initial_bounds;
  auto browser_params = Browser::CreateParams::CreateForApp(
      app_name, /*trusted_source=*/true, initial_bounds, profile,
      /*user_gesture=*/true);
  browser_params.initial_show_state = DetermineWindowShowState();
  return new Browser(browser_params);
}

void SetWebAppPrefsForWebContents(content::WebContents* web_contents) {
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();
  web_contents->NotifyPreferencesChanged();
}

content::WebContents* ShowWebApplicationWindow(
    const ::AppLaunchParams& params,
    const std::string& app_id,
    const GURL& launch_url,
    Browser* browser,
    WindowOpenDisposition disposition) {
  NavigateParams nav_params(browser, launch_url,
                            ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_params.disposition = disposition;
  nav_params.opener = params.opener;
  Navigate(&nav_params);

  content::WebContents* web_contents =
      nav_params.navigated_or_inserted_contents;

  SetWebAppPrefsForWebContents(web_contents);

  web_app::WebAppTabHelperBase* tab_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetAppId(app_id);

  browser->window()->Show();
  web_contents->SetInitialFocus();
  return web_contents;
}

}  // namespace

WebAppLaunchManager::WebAppLaunchManager(Profile* profile)
    : apps::LaunchManager(profile),
      provider_(web_app::WebAppProvider::Get(profile)) {}

WebAppLaunchManager::~WebAppLaunchManager() = default;

bool WebAppLaunchManager::OpenApplicationWindow(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  if (!provider_)
    return false;

  ::AppLaunchParams params(profile(), app_id,
                           apps::mojom::LaunchContainer::kLaunchContainerWindow,
                           WindowOpenDisposition::NEW_WINDOW,
                           apps::mojom::AppLaunchSource::kSourceCommandLine);
  params.command_line = command_line;
  params.current_directory = current_directory;

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppLaunchManager::OpenWebApplication,
                                weak_ptr_factory_.GetWeakPtr(), params));

  return true;
}

bool WebAppLaunchManager::OpenApplicationTab(const std::string& app_id) {
  if (!provider_)
    return false;

  ::AppLaunchParams params(profile(), app_id,
                           apps::mojom::LaunchContainer::kLaunchContainerTab,
                           WindowOpenDisposition::NEW_FOREGROUND_TAB,
                           apps::mojom::AppLaunchSource::kSourceCommandLine);

  // Wait for the web applications database to load.
  // If the profile and WebAppLaunchManager are destroyed,
  // on_registry_ready will not fire.
  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppLaunchManager::OpenWebApplication,
                                weak_ptr_factory_.GetWeakPtr(), params));
  return true;
}

void WebAppLaunchManager::OpenWebApplication(const ::AppLaunchParams& params) {
  if (!provider_->registrar().IsInstalled(params.app_id))
    return;

  Browser* browser = CreateWebApplicationWindow(params.profile, params.app_id);

  ShowWebApplicationWindow(
      params, params.app_id,
      provider_->registrar().GetAppLaunchURL(params.app_id), browser,
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

}  // namespace web_app
