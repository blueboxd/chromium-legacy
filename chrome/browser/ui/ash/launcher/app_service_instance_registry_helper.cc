// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_instance_registry_helper.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_contents.h"

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
    apps::InstanceState state = apps::InstanceState::kUnknown;
    proxy_->InstanceRegistry().ForOneInstance(
        old_contents->GetNativeView(),
        [&state](const apps::InstanceUpdate& update) {
          state = update.State();
        });
    state =
        static_cast<apps::InstanceState>(state & ~apps::InstanceState::kActive);
    OnInstances(launcher_controller_helper_->GetAppID(old_contents),
                old_contents->GetNativeView(), std::string(), state);
  }

  if (new_contents) {
    apps::InstanceState state = apps::InstanceState::kUnknown;
    proxy_->InstanceRegistry().ForOneInstance(
        new_contents->GetNativeView(),
        [&state](const apps::InstanceUpdate& update) {
          state = update.State();
        });
    state =
        static_cast<apps::InstanceState>(state | apps::InstanceState::kActive);
    OnInstances(launcher_controller_helper_->GetAppID(new_contents),
                new_contents->GetNativeView(), std::string(), state);
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
      apps::InstanceState::kStarted | apps::InstanceState::kRunning |
      apps::InstanceState::kActive | apps::InstanceState::kVisible);
  OnInstances(launcher_controller_helper_->GetAppID(contents),
              contents->GetNativeView(), std::string(), state);
}

void AppServiceInstanceRegistryHelper::OnTabClosing(
    content::WebContents* contents) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    return;

  OnInstances(launcher_controller_helper_->GetAppID(contents),
              contents->GetNativeView(), std::string(),
              apps::InstanceState::kDestroyed);
}

void AppServiceInstanceRegistryHelper::OnInstances(const std::string& app_id,
                                                   aura::Window* window,
                                                   const std::string& launch_id,
                                                   apps::InstanceState state) {
  if (proxy_->AppRegistryCache().GetAppType(app_id) ==
      apps::mojom::AppType::kUnknown) {
    return;
  }

  std::unique_ptr<apps::Instance> instance =
      std::make_unique<apps::Instance>(app_id, window);
  instance->SetLaunchId(launch_id);
  instance->UpdateState(state, base::Time::Now());

  std::vector<std::unique_ptr<apps::Instance>> deltas;
  deltas.push_back(std::move(instance));
  proxy_->InstanceRegistry().OnInstances(std::move(deltas));
}
