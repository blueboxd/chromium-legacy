// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/app_info_generator.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace {

em::AppInfo::Status ExtractStatus(const apps::mojom::Readiness readiness) {
  switch (readiness) {
    case apps::mojom::Readiness::kReady:
      return em::AppInfo::Status::AppInfo_Status_STATUS_INSTALLED;
    case apps::mojom::Readiness::kUninstalledByUser:
      return em::AppInfo::Status::AppInfo_Status_STATUS_UNINSTALLED;
    case apps::mojom::Readiness::kDisabledByBlacklist:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kTerminated:
      return em::AppInfo::Status::AppInfo_Status_STATUS_DISABLED;
    case apps::mojom::Readiness::kUnknown:
      return em::AppInfo::Status::AppInfo_Status_STATUS_UNKNOWN;
  }
}

em::AppInfo::AppType ExtractAppType(const apps::mojom::AppType app_type) {
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_ARC;
    case apps::mojom::AppType::kBuiltIn:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_BUILTIN;
    case apps::mojom::AppType::kCrostini:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_CROSTINI;
    case apps::mojom::AppType::kPluginVm:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_PLUGINVM;
    case apps::mojom::AppType::kExtension:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_EXTENSION;
    case apps::mojom::AppType::kWeb:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_WEB;
    case apps::mojom::AppType::kMacNative:
    case apps::mojom::AppType::kUnknown:
      return em::AppInfo::AppType::AppInfo_AppType_TYPE_UNKNOWN;
  }
}

}  // namespace

namespace policy {

AppInfoGenerator::AppInfoGenerator(Profile* profile) : profile_(profile) {}

std::vector<em::AppInfo> AppInfoGenerator::Generate() const {
  std::vector<em::AppInfo> app_infos;

  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProviderFactory::GetForProfile(profile_);
  if (!app_service_proxy || !web_app_provider) {
    LOG(WARNING) << "Could not get app providers. Returning empty app list.";
    return app_infos;
  }

  app_service_proxy->AppRegistryCache().ForEachApp(
      [&app_infos, web_app_provider](const apps::AppUpdate& update) {
        em::AppInfo info;
        auto is_web_app = update.AppType() == apps::mojom::AppType::kWeb;
        if (!is_web_app) {
          info.set_app_id(update.AppId());
          info.set_app_name(update.Name());
        } else {
          auto launch_url = web_app_provider->registrar()
                                .GetAppLaunchURL(update.AppId())
                                .GetOrigin()
                                .spec();
          info.set_app_id(launch_url);
          info.set_app_name(launch_url);
        }
        info.set_status(ExtractStatus(update.Readiness()));
        info.set_version(update.Version());
        info.set_app_type(ExtractAppType(update.AppType()));
        app_infos.push_back(info);
      });

  return app_infos;
}

}  // namespace policy
