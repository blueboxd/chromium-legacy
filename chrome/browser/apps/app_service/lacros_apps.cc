// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/lacros_apps.h"

#include <utility>

#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/chromeos/lacros/lacros_loader.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/constants.h"

namespace {

apps::mojom::AppPtr GetLacrosApp() {
  apps::mojom::AppPtr app = apps::PublisherBase::MakeApp(
      apps::mojom::AppType::kLacros, extension_misc::kLacrosAppId,
      apps::mojom::Readiness::kReady,
      "LaCrOS",  // TODO(jamescook): Localized name.
      apps::mojom::InstallSource::kSystem);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Canary icon only exists in branded builds.
  int icon_resource_id = IDR_PRODUCT_LOGO_256_CANARY;
#else
  int icon_resource_id = IDR_PRODUCT_LOGO_256;
#endif
  app->icon_key =
      apps::mojom::IconKey::New(apps::mojom::IconKey::kDoesNotChangeOverTime,
                                icon_resource_id, apps::IconEffects::kNone);
  app->searchable = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  app->show_in_search = apps::mojom::OptionalBool::kTrue;
  app->show_in_management = apps::mojom::OptionalBool::kFalse;
  return app;
}

}  // namespace

namespace apps {

LacrosApps::LacrosApps(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(chromeos::features::IsLacrosSupportEnabled());
  PublisherBase::Initialize(app_service, apps::mojom::AppType::kLacros);
}

LacrosApps::~LacrosApps() = default;

void LacrosApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(GetLacrosApp());

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void LacrosApps::LoadIcon(const std::string& app_id,
                          apps::mojom::IconKeyPtr icon_key,
                          apps::mojom::IconCompression icon_compression,
                          int32_t size_hint_in_dip,
                          bool allow_placeholder_icon,
                          LoadIconCallback callback) {
  if (icon_key &&
      icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId) {
    LoadIconFromResource(
        icon_compression, size_hint_in_dip, icon_key->resource_id,
        /*is_placeholder_icon=*/false,
        static_cast<IconEffects>(icon_key->icon_effects), std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LacrosApps::Launch(const std::string& app_id,
                        int32_t event_flags,
                        apps::mojom::LaunchSource launch_source,
                        int64_t display_id) {
  DCHECK_EQ(extension_misc::kLacrosAppId, app_id);
  LacrosLoader::Get()->Start();
}

void LacrosApps::GetMenuModel(const std::string& app_id,
                              apps::mojom::MenuType menu_type,
                              int64_t display_id,
                              GetMenuModelCallback callback) {
  // No menu items.
  std::move(callback).Run(apps::mojom::MenuItems::New());
}

}  // namespace apps
