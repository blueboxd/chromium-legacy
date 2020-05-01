// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps.h"

#include "chrome/browser/apps/app_service/extension_uninstaller.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"

namespace apps {

ExtensionApps::ExtensionApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile,
    apps::mojom::AppType app_type)
    : ExtensionAppsBase(app_service, profile, app_type) {}

ExtensionApps::~ExtensionApps() = default;

// static
void ExtensionApps::UninstallImpl(Profile* profile,
                                  const std::string& app_id,
                                  gfx::NativeWindow parent_window) {
  if (!profile) {
    return;
  }

  ExtensionUninstaller::Create(profile, app_id, parent_window);
}

bool ExtensionApps::Accepts(const extensions::Extension* extension) {
  if (!extension->is_app()) {
    return false;
  }

  switch (app_type()) {
    case apps::mojom::AppType::kExtension:
      return !extension->from_bookmark();
    case apps::mojom::AppType::kWeb:
      return extension->from_bookmark();
    default:
      NOTREACHED();
      return false;
  }
}

bool ExtensionApps::ShouldShownInLauncher(
    const extensions::Extension* extension) {
  return true;
}

apps::mojom::AppPtr ExtensionApps::Convert(
    const extensions::Extension* extension,
    apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = ConvertImpl(extension, readiness);

  app->icon_key = icon_key_factory_.MakeIconKey(GetIconEffects(extension));

  app->has_badge = apps::mojom::OptionalBool::kFalse;
  app->paused = apps::mojom::OptionalBool::kFalse;

  return app;
}

}  // namespace apps
