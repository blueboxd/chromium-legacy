// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace apps {

std::string GetAppIdForWebContents(content::WebContents* web_contents) {
  std::string app_id;

  web_app::WebAppTabHelperBase* web_app_tab_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);
  // web_app_tab_helper is nullptr in some unit tests.
  if (web_app_tab_helper) {
    app_id = web_app_tab_helper->GetAppId();
  }

  if (app_id.empty()) {
    extensions::TabHelper* extensions_tab_helper =
        extensions::TabHelper::FromWebContents(web_contents);
    // extensions_tab_helper is nullptr in some tests.
    if (extensions_tab_helper) {
      app_id = extensions_tab_helper->GetExtensionAppId();
    }
  }

  return app_id;
}

bool IsInstalledApp(Profile* profile, const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    return true;
  }
  web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
  return registrar.IsInstalled(app_id);
}

void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id) {
  extensions::TabHelper::CreateForWebContents(web_contents);
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    web_app::WebAppTabHelperBase::FromWebContents(web_contents)
        ->SetAppId(std::string());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(app_id);
  } else {
    web_app::AppRegistrar& registrar =
        web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
    web_app::WebAppTabHelperBase::FromWebContents(web_contents)
        ->SetAppId(registrar.IsInstalled(app_id) ? app_id : std::string());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionAppById(std::string());
  }
}

std::vector<base::FilePath> GetLaunchFilesFromCommandLine(
    const base::CommandLine& command_line) {
  std::vector<base::FilePath> launch_files;
  if (!command_line.HasSwitch(switches::kAppId)) {
    return launch_files;
  }

  // Assume all args passed were intended as files to pass to the app.
  launch_files.reserve(command_line.GetArgs().size());
  for (const auto& arg : command_line.GetArgs()) {
    base::FilePath path(arg);
    if (path.empty()) {
      continue;
    }

    launch_files.push_back(path);
  }

  return launch_files;
}

Browser* CreateBrowserWithNewTabPage(Profile* profile) {
  Browser::CreateParams create_params(profile, /*user_gesture=*/false);
  Browser* browser = new Browser(create_params);

  NavigateParams params(browser, GURL(chrome::kChromeUINewTabURL),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  Navigate(&params);

  browser->window()->Show();
  return browser;
}

}  // namespace apps
