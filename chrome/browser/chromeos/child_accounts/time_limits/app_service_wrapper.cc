// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"

#include <string>

#include "base/optional.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace app_time {

namespace {

// Return whether app with |app_id| should be included for per-app time
// limits.
bool ShouldIncludeApp(const AppId& app_id) {
  return app_id.app_type() == apps::mojom::AppType::kArc ||
         app_id.app_type() == apps::mojom::AppType::kWeb ||
         app_id.app_id() == extension_misc::kChromeAppId;
}

// Gets AppId from |update|.
AppId AppIdFromAppUpdate(const apps::AppUpdate& update) {
  bool is_arc = update.AppType() == apps::mojom::AppType::kArc;
  return AppId(update.AppType(),
               is_arc ? update.PublisherId() : update.AppId());
}

// Gets AppId from |update|.
AppId AppIdFromInstanceUpdate(const apps::InstanceUpdate& update,
                              apps::AppRegistryCache* app_cache) {
  base::Optional<AppId> app_id;
  app_cache->ForOneApp(update.AppId(),
                       [&app_id](const apps::AppUpdate& update) {
                         app_id = AppIdFromAppUpdate(update);
                       });
  return app_id.value();
}

// Gets app service id from |app_id|.
std::string AppServiceIdFromAppId(const AppId& app_id, Profile* profile) {
  return app_id.app_type() == apps::mojom::AppType::kArc
             ? arc::ArcPackageNameToAppId(app_id.app_id(), profile)
             : app_id.app_id();
}

}  // namespace

AppServiceWrapper::AppServiceWrapper(Profile* profile) : profile_(profile) {
  apps::AppRegistryCache::Observer::Observe(&GetAppCache());
  apps::InstanceRegistry::Observer::Observe(&GetInstanceRegistry());
}

AppServiceWrapper::~AppServiceWrapper() = default;

std::vector<AppId> AppServiceWrapper::GetInstalledApps() const {
  std::vector<AppId> installed_apps;
  GetAppCache().ForEachApp([&installed_apps](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)
      return;

    const AppId app_id = AppIdFromAppUpdate(update);
    if (!ShouldIncludeApp(app_id))
      return;

    installed_apps.push_back(app_id);
  });
  return installed_apps;
}

std::string AppServiceWrapper::GetAppName(const AppId& app_id) const {
  const std::string app_service_id = AppServiceIdFromAppId(app_id, profile_);
  DCHECK(!app_service_id.empty());

  std::string app_name;
  GetAppCache().ForOneApp(
      app_service_id,
      [&app_name](const apps::AppUpdate& update) { app_name = update.Name(); });
  return app_name;
}

std::string AppServiceWrapper::GetAppServiceId(const AppId& app_id) const {
  return AppServiceIdFromAppId(app_id, profile_);
}

void AppServiceWrapper::AddObserver(EventListener* listener) {
  DCHECK(listener);
  listeners_.AddObserver(listener);
}

void AppServiceWrapper::RemoveObserver(EventListener* listener) {
  DCHECK(listener);
  listeners_.RemoveObserver(listener);
}

void AppServiceWrapper::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged())
    return;

  const AppId app_id = AppIdFromAppUpdate(update);
  if (!ShouldIncludeApp(app_id))
    return;

  switch (update.Readiness()) {
    case apps::mojom::Readiness::kReady:
      for (auto& listener : listeners_)
        if (update.StateIsNull()) {
          // It is the first update about this app.
          // Note that AppService does not store info between sessions and this
          // will be called at the beginning of every session.
          listener.OnAppInstalled(app_id);
        } else {
          listener.OnAppAvailable(app_id);
        }
      break;
    case apps::mojom::Readiness::kUninstalledByUser:
      for (auto& listener : listeners_)
        listener.OnAppUninstalled(app_id);
      break;
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kDisabledByBlacklist:
      for (auto& listener : listeners_)
        listener.OnAppBlocked(app_id);
      break;
    default:
      break;
  }
}

void AppServiceWrapper::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void AppServiceWrapper::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged())
    return;

  const AppId app_id = AppIdFromInstanceUpdate(update, &GetAppCache());
  if (!ShouldIncludeApp(app_id))
    return;

  bool is_active = update.State() & apps::InstanceState::kActive;
  for (auto& listener : listeners_) {
    if (is_active) {
      listener.OnAppActive(app_id, update.Window(), update.LastUpdatedTime());
    } else {
      listener.OnAppInactive(app_id, update.Window(), update.LastUpdatedTime());
    }
  }
}

void AppServiceWrapper::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  apps::InstanceRegistry::Observer::Observe(nullptr);
}

apps::AppRegistryCache& AppServiceWrapper::GetAppCache() const {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  return proxy->AppRegistryCache();
}

apps::InstanceRegistry& AppServiceWrapper::GetInstanceRegistry() const {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  return proxy->InstanceRegistry();
}

}  // namespace app_time
}  // namespace chromeos
