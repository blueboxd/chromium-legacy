// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"

#include <string>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace chromeos {
namespace app_time {

namespace {

// Return whether |app| should be included for per-app time limits.
// TODO(agawronska): Add support for PWA and Chrome.
bool ShouldIncludeApp(const apps::AppUpdate& app) {
  return app.AppType() == apps::mojom::AppType::kArc;
}

// Gets AppId from |update|.
AppId AppIdFromAppUpdate(const apps::AppUpdate& update) {
  bool is_arc = update.AppType() == apps::mojom::AppType::kArc;
  return AppId(update.AppType(),
               is_arc ? update.PublisherId() : update.AppId());
}

}  // namespace

AppServiceWrapper::AppServiceWrapper(Profile* profile) : profile_(profile) {
  Observe(&GetAppCache());
}

AppServiceWrapper::~AppServiceWrapper() = default;

std::vector<AppId> AppServiceWrapper::GetInstalledApps() const {
  std::vector<AppId> installed_apps;
  GetAppCache().ForEachApp([&installed_apps](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)
      return;

    if (!ShouldIncludeApp(update))
      return;

    installed_apps.push_back(AppIdFromAppUpdate(update));
  });
  return installed_apps;
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

  if (!ShouldIncludeApp(update))
    return;

  const AppId app_id = AppIdFromAppUpdate(update);
  switch (update.Readiness()) {
    case apps::mojom::Readiness::kReady:
      for (auto& listener : listeners_)
        listener.OnAppInstalled(app_id);
      break;
    case apps::mojom::Readiness::kUninstalledByUser:
      for (auto& listener : listeners_)
        listener.OnAppUninstalled(app_id);
      break;
    default:
      break;
  }
}

void AppServiceWrapper::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

apps::AppRegistryCache& AppServiceWrapper::GetAppCache() const {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  return proxy->AppRegistryCache();
}

}  // namespace app_time
}  // namespace chromeos
