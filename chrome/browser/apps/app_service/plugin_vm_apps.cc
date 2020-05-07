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
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void SetAppAllowed(apps::mojom::App* app, bool allowed) {
  app->readiness = allowed ? apps::mojom::Readiness::kReady
                           : apps::mojom::Readiness::kDisabledByPolicy;

  const apps::mojom::OptionalBool opt_allowed =
      allowed ? apps::mojom::OptionalBool::kTrue
              : apps::mojom::OptionalBool::kFalse;

  app->recommendable = opt_allowed;
  app->searchable = opt_allowed;
  app->show_in_launcher = opt_allowed;
  app->show_in_search = opt_allowed;
}

apps::mojom::AppPtr GetPluginVmApp(bool allowed) {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kPluginVm, plugin_vm::kPluginVmAppId,
      allowed ? apps::mojom::Readiness::kReady
              : apps::mojom::Readiness::kDisabledByPolicy,
      l10n_util::GetStringUTF8(IDS_PLUGIN_VM_APP_NAME),
      apps::mojom::InstallSource::kSystem);

  app->icon_key = apps::mojom::IconKey::New(
      apps::mojom::IconKey::kDoesNotChangeOverTime,
      IDR_LOGO_PLUGIN_VM_DEFAULT_192, apps::IconEffects::kNone);

  app->show_in_management = apps::mojom::OptionalBool::kFalse;

  SetAppAllowed(app.get(), allowed);

  return app;
}

}  // namespace

namespace apps {

PluginVmApps::PluginVmApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile) {
  PublisherBase::Initialize(app_service, apps::mojom::AppType::kPluginVm);

  // Register for Plugin VM policy changes, so that we can update the
  // availability of the Plugin VM app. This subscription is automatically
  // cleaned-up when this object is destroyed.
  policy_subscription_ =
      std::make_unique<plugin_vm::PluginVmPolicySubscription>(
          profile_, base::BindRepeating(&PluginVmApps::OnPluginVmAllowedChanged,
                                        base::Unretained(this)));

  is_allowed_ = plugin_vm::IsPluginVmAllowedForProfile(profile_);
}

PluginVmApps::~PluginVmApps() = default;

void PluginVmApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetPluginVmApp(is_allowed_));

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
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
    plugin_vm::PluginVmManagerFactory::GetForProfile(profile_)->LaunchPluginVm(
        base::DoNothing());
  } else {
    plugin_vm::ShowPluginVmInstallerView(profile_);
  }
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
    AddCommandItem(ash::SHUTDOWN_GUEST_OS, IDS_PLUGIN_VM_SHUT_DOWN_MENU_ITEM,
                   &menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void PluginVmApps::OnPluginVmAllowedChanged(bool is_allowed) {
  // Republish the Plugin VM app when policy changes have changed
  // its availability. Only changed fields need to be republished.
  is_allowed_ = is_allowed;

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kPluginVm;
  app->app_id = plugin_vm::kPluginVmAppId;
  SetAppAllowed(app.get(), is_allowed);
  Publish(std::move(app), subscribers_);
}

}  // namespace apps
