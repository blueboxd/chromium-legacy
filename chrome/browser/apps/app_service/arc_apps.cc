// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/arc_apps.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_descriptor.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/grit/component_extension_resources.h"
#include "components/arc/app_permissions/arc_app_permissions_bridge.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/common/app_permissions.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/system_connector.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

// TODO(crbug.com/826982): investigate re-using the ArcAppIcon class directly.
// This may or may not be difficult: see
// (https://chromium-review.googlesource.com/c/chromium/src/+/1482350/7#message-b45fa253ea01b523e8389b30d74ce805b0e05f77)
// and
// (https://chromium-review.googlesource.com/c/chromium/src/+/1482350/7#message-52080b7d348d7806c818aa395392ff1385d1784e).

// TODO(crbug.com/826982): consider that, per khmel@, "App icon can be
// overwritten (setTaskDescription) or by assigning the icon for the app
// window. In this case some consumers (Shelf for example) switch to
// overwritten icon... IIRC this applies to shelf items and ArcAppWindow icon".

// TODO(crbug.com/826982): consider that, per khmel@, "We may change the way
// how we handle icons in ARC++ container. That means view of the icon can be
// changed. We support invalidation of icon scales way, similar to the case
// above... We have methods to notify about new icon arrival. IIRC ArcAppIcon
// already handles this".

// TODO(crbug.com/826982): consider that, per khmel@, "Functionality to detect
// icons cannot be decoded correctly and issue request to refresh the icon
// scale from ARC++ container... The logic here is wider. We request new copy
// of icon in this case".

namespace {

// ArcApps::LoadIcon (via ArcApps::LoadIconFromVM) runs a series of callbacks,
// defined here in back-to-front order so that e.g. the compiler knows
// LoadIcon2's signature when compiling LoadIcon1 (which binds LoadIcon2).
//
//  - LoadIcon0 is called back when the AppConnectionHolder is connected.
//  - LoadIcon1 is called back when the compressed (PNG) image is loaded.
//  - LoadIcon2 is called back when the uncompressed image is loaded.

void LoadIcon2(apps::mojom::Publisher::LoadIconCallback callback,
               const SkBitmap& bitmap) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 0.0f));
  std::move(callback).Run(std::move(iv));
}

void LoadIcon1(apps::mojom::IconCompression icon_compression,
               apps::mojom::Publisher::LoadIconCallback callback,
               const std::vector<uint8_t>& icon_png_data) {
  switch (icon_compression) {
    case apps::mojom::IconCompression::kUnknown:
      std::move(callback).Run(apps::mojom::IconValue::New());
      break;

    case apps::mojom::IconCompression::kUncompressed:
      data_decoder::DecodeImage(
          content::GetSystemConnector(), icon_png_data,
          data_decoder::mojom::ImageCodec::DEFAULT, false,
          data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
          base::BindOnce(&LoadIcon2, std::move(callback)));
      break;

    case apps::mojom::IconCompression::kCompressed:
      apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
      iv->icon_compression = apps::mojom::IconCompression::kCompressed;
      iv->compressed = icon_png_data;
      std::move(callback).Run(std::move(iv));
      break;
  }
}

void LoadIcon0(apps::mojom::IconCompression icon_compression,
               int size_hint_in_px,
               std::string package_name,
               std::string activity,
               std::string icon_resource_id,
               apps::mojom::Publisher::LoadIconCallback callback,
               apps::ArcApps::AppConnectionHolder* app_connection_holder) {
  // TODO(crbug.com/826982): consider that, per khmel@, "Regardless the number
  // of request for the same icon scale it should be only one and only one
  // request to ARC++ container to extract the real data. This logic is
  // isolated inside ArcAppListPrefs and I don't think that anybody else should
  // call mojom RequestAppIcon".
  if (app_connection_holder) {
    if (icon_resource_id.empty()) {
      auto* app_instance =
          ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder, RequestAppIcon);
      if (app_instance) {
        app_instance->RequestAppIcon(
            package_name, activity, size_hint_in_px,
            base::BindOnce(&LoadIcon1, icon_compression, std::move(callback)));
        return;
      }

    } else {
      auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(app_connection_holder,
                                                       RequestShortcutIcon);
      if (app_instance) {
        app_instance->RequestShortcutIcon(
            icon_resource_id, size_hint_in_px,
            base::BindOnce(&LoadIcon1, icon_compression, std::move(callback)));
        return;
      }
    }
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void UpdateAppPermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& new_permissions,
    std::vector<apps::mojom::PermissionPtr>* permissions) {
  for (const auto& new_permission : new_permissions) {
    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(new_permission.first);
    permission->value_type = apps::mojom::PermissionValueType::kBool;
    permission->value = static_cast<uint32_t>(new_permission.second->granted);
    permission->is_managed = new_permission.second->managed;

    permissions->push_back(std::move(permission));
  }
}

}  // namespace

namespace apps {

// static
ArcApps* ArcApps::Get(Profile* profile) {
  return ArcAppsFactory::GetForProfile(profile);
}

// static
ArcApps* ArcApps::CreateForTesting(Profile* profile,
                                   apps::AppServiceProxy* proxy) {
  return new ArcApps(profile, proxy);
}

ArcApps::ArcApps(Profile* profile) : ArcApps(profile, nullptr) {}

ArcApps::ArcApps(Profile* profile, apps::AppServiceProxy* proxy)
    : binding_(this), profile_(profile), prefs_(nullptr) {
  if (!arc::IsArcAllowedForProfile(profile_) ||
      (arc::ArcServiceManager::Get() == nullptr)) {
    return;
  }

  if (!proxy) {
    proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  }
  apps::mojom::AppServicePtr& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  prefs_ = ArcAppListPrefs::Get(profile);
  if (!prefs_) {
    return;
  }
  prefs_->AddObserver(this);
  prefs_->app_connection_holder()->AddObserver(this);

  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  app_service->RegisterPublisher(std::move(publisher),
                                 apps::mojom::AppType::kArc);
}

ArcApps::~ArcApps() {
  // Clear out any pending icon calls to avoid a CHECK for Mojo callbacks that
  // have not been run at App Service destruction.
  for (auto& pending : pending_load_icon_calls_) {
    std::move(pending).Run(nullptr);
  }
  pending_load_icon_calls_.clear();

  if (prefs_) {
    auto* holder = prefs_->app_connection_holder();
    // The null check is for unit tests. On production, |holder| is always
    // non-null.
    if (holder) {
      holder->RemoveObserver(this);
    }
    prefs_->RemoveObserver(this);
  }
}

void ArcApps::Connect(apps::mojom::SubscriberPtr subscriber,
                      apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  for (const auto& app_id : prefs_->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs_->GetApp(app_id);
    if (app_info) {
      apps.push_back(Convert(app_id, *app_info));
    }
  }
  subscriber->OnApps(std::move(apps));
  subscribers_.AddPtr(std::move(subscriber));
}

void ArcApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconCompression icon_compression,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  if (icon_key) {
    // Treat the Play Store as a special case, loading an icon defined by a
    // resource instead of asking the Android VM (or the cache of previous
    // responses from the Android VM). Presumably this is for bootstrapping:
    // the Play Store icon (the UI for enabling and installing Android apps)
    // should be showable even before the user has installed their first
    // Android app and before bringing up an Android VM for the first time.
    if (app_id == arc::kPlayStoreAppId) {
      LoadPlayStoreIcon(icon_compression, size_hint_in_dip,
                        static_cast<IconEffects>(icon_key->icon_effects),
                        std::move(callback));
      return;
    }

    // Try loading the icon from an on-disk cache. If that fails, fall back to
    // LoadIconFromVM.
    LoadIconFromFileWithFallback(
        icon_compression, size_hint_in_dip,
        GetCachedIconFilePath(app_id, size_hint_in_dip),
        static_cast<IconEffects>(icon_key->icon_effects), std::move(callback),
        base::BindOnce(&ArcApps::LoadIconFromVM, weak_ptr_factory_.GetWeakPtr(),
                       app_id, icon_compression, size_hint_in_dip,
                       allow_placeholder_icon,
                       static_cast<IconEffects>(icon_key->icon_effects)));
    return;
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void ArcApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     int64_t display_id) {
  auto uit = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      uit = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
    case apps::mojom::LaunchSource::kFromKioskNextHome:
      uit = arc::UserInteractionType::APP_STARTED_FROM_KIOSK_NEXT_HOME;
      break;
    case apps::mojom::LaunchSource::kFromParentalControls:
      uit = arc::UserInteractionType::APP_STARTED_FROM_SETTINGS;
      break;
  }

  arc::LaunchApp(profile_, app_id, event_flags, uit, display_id);
}

void ArcApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs_->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "SetPermission failed, could not find app with id " << app_id;
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(WARNING) << "SetPermission failed, ArcServiceManager not available.";
    return;
  }

  auto permission_type =
      static_cast<arc::mojom::AppPermission>(permission->permission_id);
  if (permission->value) {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        GrantPermission);
    if (permissions_instance) {
      permissions_instance->GrantPermission(app_info->package_name,
                                            permission_type);
    }
  } else {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        RevokePermission);
    if (permissions_instance) {
      permissions_instance->RevokePermission(app_info->package_name,
                                             permission_type);
    }
  }
}

void ArcApps::Uninstall(const std::string& app_id) {
  if (!profile_) {
    return;
  }
  arc::ShowArcAppUninstallDialog(profile_, app_id);
}

void ArcApps::OpenNativeSettings(const std::string& app_id) {
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs_->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Cannot open native settings for " << app_id
               << ". App is not found.";
    return;
  }
  arc::ShowPackageInfo(app_info->package_name,
                       arc::mojom::ShowPackageInfoPage::MAIN,
                       display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void ArcApps::OnConnectionReady() {
  AppConnectionHolder* app_connection_holder = prefs_->app_connection_holder();
  for (auto& pending : pending_load_icon_calls_) {
    std::move(pending).Run(app_connection_holder);
  }
  pending_load_icon_calls_.clear();
}

void ArcApps::OnAppRegistered(const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info) {
  Publish(Convert(app_id, app_info));
}

void ArcApps::OnAppStatesChanged(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info) {
  Publish(Convert(app_id, app_info));
}

void ArcApps::OnAppRemoved(const std::string& app_id) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;
  Publish(std::move(app));
}

void ArcApps::OnAppIconUpdated(const std::string& app_id,
                               const ArcAppIconDescriptor& descriptor) {
  static constexpr uint32_t icon_effects = 0;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  Publish(std::move(app));
}

void ArcApps::OnAppNameUpdated(const std::string& app_id,
                               const std::string& name) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->name = name;
  Publish(std::move(app));
}

void ArcApps::OnAppLastLaunchTimeUpdated(const std::string& app_id) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs_->GetApp(app_id);
  if (app_info) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kArc;
    app->app_id = app_id;
    app->last_launch_time = app_info->last_launch_time;
    Publish(std::move(app));
  }
}

void ArcApps::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ConvertAndPublishPackageApps(package_info);
}

void ArcApps::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  ConvertAndPublishPackageApps(package_info);
}

void ArcApps::OnPackageListInitialRefreshed() {
  for (const auto& app_id : prefs_->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs_->GetApp(app_id);
    if (app_info) {
      Publish(Convert(app_id, *app_info));
    }
  }
}

const base::FilePath ArcApps::GetCachedIconFilePath(const std::string& app_id,
                                                    int32_t size_hint_in_dip) {
  // TODO(crbug.com/826982): process the app_id argument like the private
  // GetAppFromAppOrGroupId function and the ArcAppIcon::mapped_app_id_ field
  // in arc_app_icon.cc?
  return prefs_->GetIconPath(
      app_id,
      ArcAppIconDescriptor(size_hint_in_dip,
                           apps_util::GetPrimaryDisplayUIScaleFactor()));
}

void ArcApps::LoadIconFromVM(const std::string app_id,
                             apps::mojom::IconCompression icon_compression,
                             int32_t size_hint_in_dip,
                             bool allow_placeholder_icon,
                             IconEffects icon_effects,
                             LoadIconCallback callback) {
  if (allow_placeholder_icon) {
    constexpr bool is_placeholder_icon = true;
    LoadIconFromResource(icon_compression, size_hint_in_dip,
                         IDR_APP_DEFAULT_ICON, is_placeholder_icon,
                         icon_effects, std::move(callback));
    return;
  }

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs_->GetApp(app_id);
  if (app_info) {
    base::OnceCallback<void(apps::ArcApps::AppConnectionHolder*)> pending =
        base::BindOnce(&LoadIcon0, icon_compression,
                       apps_util::ConvertDipToPx(size_hint_in_dip),
                       app_info->package_name, app_info->activity,
                       app_info->icon_resource_id, std::move(callback));

    AppConnectionHolder* app_connection_holder =
        prefs_->app_connection_holder();
    if (app_connection_holder->IsConnected()) {
      std::move(pending).Run(app_connection_holder);
    } else {
      pending_load_icon_calls_.push_back(std::move(pending));
    }
    return;
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void ArcApps::LoadPlayStoreIcon(apps::mojom::IconCompression icon_compression,
                                int32_t size_hint_in_dip,
                                IconEffects icon_effects,
                                LoadIconCallback callback) {
  // Use overloaded Chrome icon for Play Store that is adapted to Chrome style.
  int size_hint_in_px = apps_util::ConvertDipToPx(size_hint_in_dip);
  int resource_id = (size_hint_in_px <= 32) ? IDR_ARC_SUPPORT_ICON_32
                                            : IDR_ARC_SUPPORT_ICON_192;
  constexpr bool is_placeholder_icon = false;
  LoadIconFromResource(icon_compression, size_hint_in_dip, resource_id,
                       is_placeholder_icon, icon_effects, std::move(callback));
}

apps::mojom::InstallSource GetInstallSource(const ArcAppListPrefs* prefs,
                                            const std::string& package_name) {
  if (prefs->IsDefault(package_name)) {
    return apps::mojom::InstallSource::kDefault;
  }

  if (prefs->IsOem(package_name)) {
    return apps::mojom::InstallSource::kOem;
  }

  if (prefs->IsControlledByPolicy(package_name)) {
    return apps::mojom::InstallSource::kPolicy;
  }

  return apps::mojom::InstallSource::kUser;
}

apps::mojom::AppPtr ArcApps::Convert(const std::string& app_id,
                                     const ArcAppListPrefs::AppInfo& app_info) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->readiness = app_info.suspended
                       ? apps::mojom::Readiness::kDisabledByPolicy
                       : apps::mojom::Readiness::kReady;
  app->name = app_info.name;
  app->short_name = app->name;

  IconEffects icon_effects = IconEffects::kNone;
  if (app_info.suspended) {
    icon_effects = static_cast<IconEffects>(icon_effects | IconEffects::kGray);
  }
  app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;

  app->install_source = GetInstallSource(prefs_, app_info.package_name);

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;

  auto show = app_info.show_in_launcher ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management = show;

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs_->GetPackage(app_info.package_name);
  if (package) {
    UpdateAppPermissions(package->permissions, &app->permissions);
  }

  return app;
}

void ArcApps::Publish(apps::mojom::AppPtr app) {
  subscribers_.ForAllPtrs([&app](apps::mojom::Subscriber* subscriber) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  });
}

void ArcApps::ConvertAndPublishPackageApps(
    const arc::mojom::ArcPackageInfo& package_info) {
  if (!package_info.permissions.has_value()) {
    return;
  }
  std::vector<apps::mojom::AppPtr> apps;
  for (const auto& app_id :
       prefs_->GetAppsForPackage(package_info.package_name)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs_->GetApp(app_id);
    if (app_info) {
      Publish(Convert(app_id, *app_info));
    }
  }
}

}  // namespace apps
