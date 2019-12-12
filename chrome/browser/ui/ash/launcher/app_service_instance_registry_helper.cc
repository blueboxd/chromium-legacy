// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_instance_registry_helper.h"

#include <set>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"

AppServiceInstanceRegistryHelper::AppServiceInstanceRegistryHelper(
    Profile* profile)
    : proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)),
      launcher_controller_helper_(
          std::make_unique<LauncherControllerHelper>(profile)) {}

AppServiceInstanceRegistryHelper::~AppServiceInstanceRegistryHelper() = default;

void AppServiceInstanceRegistryHelper::ActiveUserChanged() {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  proxy_ = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}

void AppServiceInstanceRegistryHelper::OnActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  if (old_contents) {
    std::string app_id = launcher_controller_helper_->GetAppID(old_contents);
    // If app_id is empty, we should not set it as inactive because this is
    // Chrome's tab.
    if (!app_id.empty()) {
      apps::InstanceState state = apps::InstanceState::kUnknown;
      proxy_->InstanceRegistry().ForOneInstance(
          old_contents->GetNativeView(),
          [&state](const apps::InstanceUpdate& update) {
            state = update.State();
          });
      // If the app has been inactive, we don't need to update the instance.
      if ((state & apps::InstanceState::kActive) !=
          apps::InstanceState::kUnknown) {
        state = static_cast<apps::InstanceState>(state &
                                                 ~apps::InstanceState::kActive);
        OnInstances(app_id, GetWindow(old_contents), std::string(), state);
      }
    }
  }

  if (new_contents) {
    // If the app is active, it should be started, running, and visible.
    apps::InstanceState state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
    OnInstances(GetAppId(new_contents), GetWindow(new_contents), std::string(),
                state);
  }
}

void AppServiceInstanceRegistryHelper::OnTabReplaced(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  OnTabClosing(old_contents);
  OnTabInserted(new_contents);
}

void AppServiceInstanceRegistryHelper::OnTabInserted(
    content::WebContents* contents) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  apps::InstanceState state = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  OnInstances(GetAppId(contents), GetWindow(contents), std::string(), state);
}

void AppServiceInstanceRegistryHelper::OnTabClosing(
    content::WebContents* contents) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  std::string app_id = launcher_controller_helper_->GetAppID(contents);
  // If app_id is empty, we should monitor the browser for Chrome.
  if (app_id.empty())
    return;

  OnInstances(app_id, GetWindow(contents), std::string(),
              apps::InstanceState::kDestroyed);
}

void AppServiceInstanceRegistryHelper::OnBrowserRemoved() {
  std::set<aura::Window*> windows =
      proxy_->InstanceRegistry().GetWindows(extension_misc::kChromeAppId);
  for (auto* window : windows) {
    if (!chrome::FindBrowserWithWindow(window)) {
      // The browser is removed if the window can't be found, so update the
      // Chrome window instance as destroyed.
      OnInstances(extension_misc::kChromeAppId, window, std::string(),
                  apps::InstanceState::kDestroyed);
    }
  }
}

void AppServiceInstanceRegistryHelper::OnInstances(const std::string& app_id,
                                                   aura::Window* window,
                                                   const std::string& launch_id,
                                                   apps::InstanceState state) {
  if (app_id.empty())
    return;

  std::unique_ptr<apps::Instance> instance =
      std::make_unique<apps::Instance>(app_id, window);
  instance->SetLaunchId(launch_id);
  instance->UpdateState(state, base::Time::Now());

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  deltas.push_back(std::move(instance));
  proxy_->InstanceRegistry().OnInstances(std::move(deltas));
}

bool AppServiceInstanceRegistryHelper::IsWebApp(
    const std::string& app_id) const {
  apps::mojom::AppType app_type = proxy_->AppRegistryCache().GetAppType(app_id);
  if (app_type != apps::mojom::AppType::kExtension &&
      app_type != apps::mojom::AppType::kWeb)
    return false;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id)
      return true;
  }
  return false;
}

std::string AppServiceInstanceRegistryHelper::GetAppId(
    content::WebContents* contents) const {
  std::string app_id = launcher_controller_helper_->GetAppID(contents);
  if (!app_id.empty())
    return app_id;
  return extension_misc::kChromeAppId;
}

aura::Window* AppServiceInstanceRegistryHelper::GetWindow(
    content::WebContents* contents) {
  std::string app_id = launcher_controller_helper_->GetAppID(contents);
  aura::Window* window = contents->GetNativeView();

  // If |app_id| is empty, it is a browser tab. Returns the toplevel window in
  // this case.
  if (app_id.empty())
    window = window->GetToplevelWindow();
  return window;
}
