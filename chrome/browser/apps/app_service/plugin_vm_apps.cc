// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/plugin_vm_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

apps::mojom::AppPtr GetPluginVmApp(bool allowed) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kPluginVm;
  app->app_id = plugin_vm::kPluginVmAppId;
  app->readiness = allowed ? apps::mojom::Readiness::kReady
                           : apps::mojom::Readiness::kDisabledByPolicy;
  app->name = l10n_util::GetStringUTF8(IDS_PLUGIN_VM_APP_NAME);
  app->short_name = app->name;

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_PLUGIN_VM_DEFAULT_192, apps::IconEffects::kNone);

  app->last_launch_time = base::Time();
  app->install_time = base::Time();

  app->install_source = apps::mojom::InstallSource::kSystem;

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;

  app->show_in_management = apps::mojom::OptionalBool::kFalse;
  app->paused = apps::mojom::OptionalBool::kFalse;

  const apps::mojom::OptionalBool opt_allowed =
      allowed ? apps::mojom::OptionalBool::kTrue
              : apps::mojom::OptionalBool::kFalse;

  app->recommendable = opt_allowed;
  app->searchable = opt_allowed;
  app->show_in_launcher = opt_allowed;
  app->show_in_search = opt_allowed;
  return app;
}

}  // namespace

namespace apps {

PluginVmApps::PluginVmApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile) {
  Initialize(app_service);
}

PluginVmApps::~PluginVmApps() = default;

void PluginVmApps::FlushMojoCallsForTesting() {
  receiver_.FlushForTesting();
}

void PluginVmApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                 apps::mojom::AppType::kPluginVm);
}

void PluginVmApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  bool is_allowed = plugin_vm::IsPluginVmAllowedForProfile(profile_);
  apps.push_back(GetPluginVmApp(is_allowed));

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
}

void PluginVmApps::LoadIcon(const std::string& app_id,
                            apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconCompression icon_compression,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  if (icon_key &&
      (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId)) {
    LoadIconFromResource(icon_compression, size_hint_in_dip,
                         icon_key->resource_id, is_placeholder_icon,
                         static_cast<IconEffects>(icon_key->icon_effects),
                         std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void PluginVmApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          int64_t display_id) {
  DCHECK_EQ(plugin_vm::kPluginVmAppId, app_id);
  if (plugin_vm::IsPluginVmEnabled(profile_)) {
    plugin_vm::PluginVmManager::GetForProfile(profile_)->LaunchPluginVm();
  } else {
    plugin_vm::ShowPluginVmInstallerView(profile_);
  }
}

void PluginVmApps::LaunchAppWithFiles(const std::string& app_id,
                                      apps::mojom::LaunchContainer container,
                                      int32_t event_flags,
                                      apps::mojom::LaunchSource launch_source,
                                      apps::mojom::FilePathsPtr file_paths) {
  NOTIMPLEMENTED();
}

void PluginVmApps::LaunchAppWithIntent(const std::string& app_id,
                                       int32_t event_flags,
                                       apps::mojom::IntentPtr intent,
                                       apps::mojom::LaunchSource launch_source,
                                       int64_t display_id) {
  NOTIMPLEMENTED();
}

void PluginVmApps::SetPermission(const std::string& app_id,
                                 apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void PluginVmApps::Uninstall(const std::string& app_id,
                             bool clear_site_data,
                             bool report_abuse) {
  LOG(ERROR) << "Uninstall failed, could not remove Plugin VM app with id "
             << app_id;
}

void PluginVmApps::PauseApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PluginVmApps::UnpauseApps(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PluginVmApps::GetMenuModel(const std::string& app_id,
                                apps::mojom::MenuType menu_type,
                                int64_t display_id,
                                GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   &menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  if (app_id == plugin_vm::kPluginVmAppId &&
      plugin_vm::IsPluginVmRunning(profile_)) {
    AddCommandItem(ash::STOP_APP, IDS_PLUGIN_VM_SHUT_DOWN_MENU_ITEM,
                   &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void PluginVmApps::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void PluginVmApps::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) {
  NOTIMPLEMENTED();
}
}  // namespace apps
