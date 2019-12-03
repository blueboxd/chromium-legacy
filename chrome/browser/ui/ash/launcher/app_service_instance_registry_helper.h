// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_

#include <memory>

#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/services/app_service/public/cpp/instance.h"

namespace apps {
class AppServiceProxy;
}

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

class Profile;

// The helper class to operate the App Service Instance Registry.
class AppServiceInstanceRegistryHelper {
 public:
  explicit AppServiceInstanceRegistryHelper(Profile* profile);
  ~AppServiceInstanceRegistryHelper();

  void ActiveUserChanged();

  // Notifies the AppService InstanceRegistry that active tabs are changed.
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents);

  // Notifies the AppService InstanceRegistry that the tab's contents are
  // changed. The |old_contents|'s instance will be removed, and the
  // |new_contents|'s instance will be added.
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents);

  // Notifies the AppService InstanceRegistry that a new tab is inserted. A new
  // instance will be add tp App Service InstanceRegistry.
  void OnTabInserted(content::WebContents* contents);

  // Notifies the AppService InstanceRegistry that the tab is closed. The
  // instance will be removed from App Service InstanceRegistry.
  void OnTabClosing(content::WebContents* contents);

  // Helper function to update App Service InstanceRegistry.
  void OnInstances(const std::string& app_id,
                   aura::Window* window,
                   const std::string& launch_id,
                   apps::InstanceState state);

 private:
  apps::AppServiceProxy* proxy_ = nullptr;

  // Used to get app info for tabs.
  std::unique_ptr<LauncherControllerHelper> launcher_controller_helper_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceInstanceRegistryHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_INSTANCE_REGISTRY_HELPER_H_
