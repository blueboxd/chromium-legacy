// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/launch_service/extension_app_launch_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/platform_app_launch.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace apps {

namespace {

void RecordBookmarkLaunch(Profile* profile, const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (extension && extension->from_bookmark())
    web_app::RecordAppWindowLaunch(profile, app_id);
}

}  // namespace

ExtensionAppLaunchManager::ExtensionAppLaunchManager(Profile* profile)
    : LaunchManager(profile) {}

ExtensionAppLaunchManager::~ExtensionAppLaunchManager() = default;

content::WebContents* ExtensionAppLaunchManager::OpenApplication(
    const AppLaunchParams& params) {
  if (params.container == apps::mojom::LaunchContainer::kLaunchContainerWindow)
    RecordBookmarkLaunch(profile(), params.app_id);
  return ::OpenApplication(profile(), params);
}

void ExtensionAppLaunchManager::LaunchApplication(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  apps::mojom::LaunchContainer container;
  if (OpenExtensionApplicationWindow(profile(), app_id, command_line,
                                     current_directory)) {
    RecordBookmarkLaunch(profile(), app_id);
    container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
  } else if (OpenExtensionApplicationTab(profile(), app_id)) {
    container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  } else {
    // Open an empty browser window as the app_id is invalid.
    CreateBrowserWithNewTabPage(profile());
    container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
  std::move(callback).Run(BrowserList::GetInstance()->GetLastActive(),
                          container);
}

}  // namespace apps
