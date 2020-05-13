// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_instance_registry_helper.h"

#include <set>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"

AppServiceInstanceRegistryHelper::AppServiceInstanceRegistryHelper(
    AppServiceAppWindowLauncherController* controller)
    : controller_(controller),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(
          controller->owner()->profile())),
      launcher_controller_helper_(std::make_unique<LauncherControllerHelper>(
          controller->owner()->profile())) {
  DCHECK(controller_);
  DCHECK(proxy_);
}

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
    auto* window = old_contents->GetNativeView();

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = proxy_->InstanceRegistry().GetShelfId(window).app_id;
    if (app_id.empty())
      app_id = launcher_controller_helper_->GetAppID(old_contents);

    // If app_id is empty, we should not set it as inactive because this is
    // Chrome's tab.
    if (!app_id.empty()) {
      apps::InstanceState state = proxy_->InstanceRegistry().GetState(window);
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
    auto* window = GetWindow(new_contents);

    // Get the app_id from the existed instance first. If there is no record for
    // the window, get the app_id from contents. Because when Chrome app open
    // method is changed from windows to tabs, the app_id could be changed based
    // on the URL, e.g. google photos, which might cause instance app_id
    // inconsistent DCHECK error.
    std::string app_id = proxy_->InstanceRegistry().GetShelfId(window).app_id;
    if (app_id.empty())
      app_id = GetAppId(new_contents);

    // If the app is active, it should be started, running, and visible.
    apps::InstanceState state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
    OnInstances(app_id, window, std::string(), state);
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

  std::string app_id = GetAppId(contents);
  aura::Window* window = GetWindow(contents);

  // When the user drags a tab to a new browser, or to an other browser, it
  // could generate a temp instance for this window with the Chrome application
  // app_id. For this case, this temp instance can be deleted, otherwise, DCHECK
  // error for inconsistent app_id.
  std::string old_app_id = app_id;
  proxy_->InstanceRegistry().ForOneInstance(
      window, [&old_app_id](const apps::InstanceUpdate& update) {
        old_app_id = update.AppId();
      });
  if (app_id != old_app_id) {
    OnInstances(old_app_id, window, std::string(),
                apps::InstanceState::kDestroyed);
  }

  AddTabWindow(app_id, window);
  apps::InstanceState state = static_cast<apps::InstanceState>(
      apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  OnInstances(app_id, window, std::string(), state);
}

void AppServiceInstanceRegistryHelper::OnTabClosing(
    content::WebContents* contents) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  aura::Window* window = GetWindow(contents);

  // When the tab is closed, if the window does not exists in the AppService
  // InstanceRegistry, we don't need to update the status.
  if (!proxy_->InstanceRegistry().Exists(window))
    return;

  std::string app_id = proxy_->InstanceRegistry().GetShelfId(window).app_id;
  RemoveTabWindow(app_id, window);
  OnInstances(app_id, window, std::string(), apps::InstanceState::kDestroyed);
}

void AppServiceInstanceRegistryHelper::OnBrowserRemoved() {
  std::set<aura::Window*> windows =
      proxy_->InstanceRegistry().GetWindows(extension_misc::kChromeAppId);
  for (auto* window : windows) {
    if (!chrome::FindBrowserWithWindow(window)) {
      // The browser is removed if the window can't be found, so update the
      // Chrome window instance as destroyed.
      browser_window_to_tab_window_.erase(window);
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

  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies. If the instance is found from a
  // proxy, still save to that proxy, otherwise, save to the current active user
  // profile's proxy.
  auto* proxy = proxy_;
  for (auto* profile : controller_->GetProfileList()) {
    auto* proxy_for_profile =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    if (proxy_for_profile->InstanceRegistry().Exists(window)) {
      proxy = proxy_for_profile;
      break;
    }
  }
  proxy->InstanceRegistry().OnInstances(std::move(deltas));
}

void AppServiceInstanceRegistryHelper::OnWindowVisibilityChanged(
    const ash::ShelfID& shelf_id,
    aura::Window* window,
    bool visible) {
  if (shelf_id.app_id != extension_misc::kChromeAppId) {
    // For Web apps opened in an app window, we need to find the top level
    // window to compare with the parameter |window|, because we save the tab
    // window in AppService InstanceRegistry for Web apps, and we should set the
    // state for the tab window to keep one instance for the Web app.
    std::set<aura::Window*> windows =
        proxy_->InstanceRegistry().GetWindows(shelf_id.app_id);
    for (auto* it : windows) {
      if (it->GetToplevelWindow() != window)
        continue;
      apps::InstanceState state = CalculateVisibilityState(it, visible);
      OnInstances(shelf_id.app_id, it, shelf_id.launch_id, state);
      return;
    }
    return;
  }

  if (!base::Contains(browser_window_to_tab_window_, window))
    return;

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (auto* it : browser_window_to_tab_window_[window]) {
    if (!proxy_->InstanceRegistry().Exists(it))
      continue;

    std::string app_id = proxy_->InstanceRegistry().GetShelfId(it).app_id;
    apps::InstanceState state = CalculateVisibilityState(it, visible);
    OnInstances(app_id, it, std::string(), state);
  }

  apps::InstanceState state = CalculateVisibilityState(window, visible);
  OnInstances(extension_misc::kChromeAppId, window, std::string(), state);
}

void AppServiceInstanceRegistryHelper::SetWindowActivated(
    const ash::ShelfID& shelf_id,
    aura::Window* window,
    bool active) {
  if (shelf_id.app_id != extension_misc::kChromeAppId) {
    // For Web apps opened in an app window, we need to find the top level
    // window to compare with |window|, because we save the tab
    // window in AppService InstanceRegistry for Web apps, and we should set the
    // state for the tab window to keep one instance for the Web app.
    std::set<aura::Window*> windows =
        proxy_->InstanceRegistry().GetWindows(shelf_id.app_id);
    for (auto* it : windows) {
      if (it->GetToplevelWindow() != window)
        continue;
      apps::InstanceState state = CalculateActivatedState(it, active);
      OnInstances(shelf_id.app_id, it, shelf_id.launch_id, state);
      return;
    }
    return;
  }

  if (!base::Contains(browser_window_to_tab_window_, window))
    return;

  // For the Chrome browser, when the window is activated, the active tab is set
  // as started, running, visible and active state.
  if (active) {
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (!browser)
      return;
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    apps::InstanceState state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
    auto* contents_window = GetWindow(contents);

    // Get the app_id from the existed instance first. The app_id for PWAs could
    // be changed based on the URL, e.g. google photos, which might cause
    // instance app_id inconsistent DCHECK error.
    std::string app_id =
        proxy_->InstanceRegistry().GetShelfId(contents_window).app_id;
    OnInstances(app_id.empty() ? GetAppId(contents) : app_id, contents_window,
                std::string(), state);
    return;
  }

  // For Chrome browser app windows, sets the state for each tab window instance
  // in this browser.
  for (auto* it : browser_window_to_tab_window_[window]) {
    if (!proxy_->InstanceRegistry().Exists(it))
      continue;
    std::string app_id = proxy_->InstanceRegistry().GetShelfId(it).app_id;
    apps::InstanceState state = CalculateActivatedState(it, active);
    OnInstances(app_id, it, std::string(), state);
  }

  apps::InstanceState state = CalculateActivatedState(window, active);
  OnInstances(extension_misc::kChromeAppId, window, std::string(), state);
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateVisibilityState(
    aura::Window* window,
    bool visible) const {
  apps::InstanceState state = proxy_->InstanceRegistry().GetState(window);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state = (visible) ? static_cast<apps::InstanceState>(
                          state | apps::InstanceState::kVisible)
                    : static_cast<apps::InstanceState>(
                          state & ~(apps::InstanceState::kVisible));
  return state;
}

apps::InstanceState AppServiceInstanceRegistryHelper::CalculateActivatedState(
    aura::Window* window,
    bool active) const {
  // If the app is active, it should be started, running, and visible.
  if (active) {
    return static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
  }

  apps::InstanceState state = proxy_->InstanceRegistry().GetState(window);
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state =
      static_cast<apps::InstanceState>(state & ~apps::InstanceState::kActive);
  return state;
}

bool AppServiceInstanceRegistryHelper::IsOpenedInBrowser(
    const std::string& app_id,
    aura::Window* window) const {
  // Crostini Terminal App with the app_id kCrostiniTerminalSystemAppId is a
  // System Web App.
  if (app_id == crostini::kCrostiniTerminalSystemAppId)
    return true;

  apps::mojom::AppType app_type = proxy_->AppRegistryCache().GetAppType(app_id);
  if (app_type != apps::mojom::AppType::kExtension &&
      app_type != apps::mojom::AppType::kWeb) {
    return false;
  }

  // For Extension apps, and Web apps, AppServiceAppWindowLauncherController
  // should only handle Chrome apps, managed by extensions::AppWindow, which
  // should set |browser_context| in AppService InstanceRegistry. So if
  // |browser_context| is not null, the app is a Chrome app,
  // AppServiceAppWindowLauncherController should handle it, otherwise, it is
  // opened in a browser, and AppServiceAppWindowLauncherController should skip
  // them.
  //
  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all proxies.
  for (auto* profile : controller_->GetProfileList()) {
    content::BrowserContext* browser_context = nullptr;
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    bool found = false;
    proxy->InstanceRegistry().ForOneInstance(
        window, [&browser_context, &found](const apps::InstanceUpdate& update) {
          browser_context = update.BrowserContext();
          found = true;
        });
    if (!found)
      continue;
    return (browser_context) ? false : true;
  }
  return true;
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

void AppServiceInstanceRegistryHelper::AddTabWindow(const std::string& app_id,
                                                    aura::Window* window) {
  if (app_id == extension_misc::kChromeAppId)
    return;

  aura::Window* top_level_window = window->GetToplevelWindow();
  browser_window_to_tab_window_[top_level_window].insert(window);
}

void AppServiceInstanceRegistryHelper::RemoveTabWindow(
    const std::string& app_id,
    aura::Window* window) {
  if (app_id == extension_misc::kChromeAppId)
    return;

  aura::Window* top_level_window = window->GetToplevelWindow();
  browser_window_to_tab_window_[top_level_window].erase(window);
}
