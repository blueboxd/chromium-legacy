// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"

using apps::mojom::OptionalBool;

namespace {

constexpr char const* kAppIdsWithHiddenMoreSettings[] = {
    extension_misc::kFilesManagerAppId,
    extensions::kWebStoreAppId,
};

constexpr char const* kAppIdsWithHiddenPinToShelf[] = {
  extension_misc::kChromeAppId,
};

app_management::mojom::ExtensionAppPermissionMessagePtr
CreateExtensionAppPermissionMessage(
    const extensions::PermissionMessage& message) {
  std::vector<std::string> submessages;
  for (const auto& submessage : message.submessages()) {
    submessages.push_back(base::UTF16ToUTF8(submessage));
  }
  return app_management::mojom::ExtensionAppPermissionMessage::New(
      base::UTF16ToUTF8(message.message()), std::move(submessages));
}

bool ShouldHideMoreSettings(const std::string app_id) {
  return base::Contains(kAppIdsWithHiddenMoreSettings, app_id);
}

bool ShouldHidePinToShelf(const std::string app_id) {
  return base::Contains(kAppIdsWithHiddenPinToShelf, app_id);
}


}  // namespace

AppManagementPageHandler::AppManagementPageHandler(
    app_management::mojom::PageHandlerRequest request,
    app_management::mojom::PagePtr page,
    content::WebUI* web_ui)
    : binding_(this, std::move(request)),
      page_(std::move(page)),
      profile_(Profile::FromWebUI(web_ui))
#if defined(OS_CHROMEOS)
      ,
      shelf_delegate_(this)
#endif
{
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  Observe(&proxy->AppRegistryCache());
}

AppManagementPageHandler::~AppManagementPageHandler() {}

void AppManagementPageHandler::OnPinnedChanged(const std::string& app_id,
                                               bool pinned) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  app_management::mojom::AppPtr app;

  proxy->AppRegistryCache().ForOneApp(
      app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kReady)
          app = CreateUIAppPtr(update);
      });

  // If an app with this id is not already installed, do nothing.
  if (!app)
    return;

  app->is_pinned = pinned ? OptionalBool::kTrue : OptionalBool::kFalse;

  page_->OnAppChanged(std::move(app));
}

void AppManagementPageHandler::GetApps(GetAppsCallback callback) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  std::vector<app_management::mojom::AppPtr> apps;
  proxy->AppRegistryCache().ForEachApp(
      [this, &apps](const apps::AppUpdate& update) {
        if (update.ShowInManagement() == apps::mojom::OptionalBool::kTrue) {
          apps.push_back(CreateUIAppPtr(update));
        }
      });

  std::move(callback).Run(std::move(apps));
}

void AppManagementPageHandler::GetExtensionAppPermissionMessages(
    const std::string& app_id,
    GetExtensionAppPermissionMessagesCallback callback) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* extension = registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::ENABLED |
                  extensions::ExtensionRegistry::DISABLED |
                  extensions::ExtensionRegistry::BLACKLISTED);
  std::vector<app_management::mojom::ExtensionAppPermissionMessagePtr> messages;
  if (extension) {
    for (const auto& message :
         extension->permissions_data()->GetPermissionMessages()) {
      messages.push_back(CreateExtensionAppPermissionMessage(message));
    }
  }
  std::move(callback).Run(std::move(messages));
}

void AppManagementPageHandler::SetPinned(const std::string& app_id,
                                         OptionalBool pinned) {
#if defined(OS_CHROMEOS)
  shelf_delegate_.SetPinned(app_id, pinned);
#else
  NOTREACHED();
#endif
}

void AppManagementPageHandler::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  proxy->SetPermission(app_id, std::move(permission));
}

void AppManagementPageHandler::Uninstall(const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  proxy->Uninstall(app_id);
}

void AppManagementPageHandler::OpenNativeSettings(const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  proxy->OpenNativeSettings(app_id);
}

app_management::mojom::AppPtr AppManagementPageHandler::CreateUIAppPtr(
    const apps::AppUpdate& update) {
  base::flat_map<uint32_t, apps::mojom::PermissionPtr> permissions;
  for (const auto& permission : update.Permissions()) {
    permissions[permission->permission_id] = permission->Clone();
  }

  auto app = app_management::mojom::App::New();
  app->id = update.AppId();
  app->type = update.AppType();
  app->title = update.Name();
  app->permissions = std::move(permissions);
  app->install_source = update.InstallSource();

  app->description = update.Description();

  // On other OS's, is_pinned defaults to OptionalBool::kUnknown, which is
  // used to represent the fact that there is no concept of being pinned.
#if defined(OS_CHROMEOS)
  app->is_pinned = shelf_delegate_.IsPinned(update.AppId())
                       ? OptionalBool::kTrue
                       : OptionalBool::kFalse;
  app->is_policy_pinned = shelf_delegate_.IsPolicyPinned(update.AppId())
                              ? OptionalBool::kTrue
                              : OptionalBool::kFalse;
#endif

  app->hide_more_settings = ShouldHideMoreSettings(app->id);
  app->hide_pin_to_shelf = ShouldHidePinToShelf(app->id);

  return app;
}

void AppManagementPageHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kUninstalledByUser) {
    page_->OnAppRemoved(update.AppId());
    return;
  }

  if (update.ShowInManagement() != apps::mojom::OptionalBool::kTrue) {
    return;
  }

  if (update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kReady) {
    page_->OnAppAdded(CreateUIAppPtr(update));
  } else {
    page_->OnAppChanged(CreateUIAppPtr(update));
  }
}

void AppManagementPageHandler::OnArcSupportChanged(bool supported) {
  page_->OnArcSupportChanged(supported);
}

void AppManagementPageHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}
