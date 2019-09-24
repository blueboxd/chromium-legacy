// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registry_update.h"

#include "base/bind_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

WebAppRegistryUpdate::WebAppRegistryUpdate(
    WebAppRegistrarMutable* mutable_registrar)
    : mutable_registrar_(mutable_registrar) {}

WebAppRegistryUpdate::~WebAppRegistryUpdate() = default;

WebApp* WebAppRegistryUpdate::UpdateApp(const AppId& app_id) {
  WebApp* app = mutable_registrar_->GetAppByIdMutable(app_id);
  if (app)
    apps_to_update_.insert(app);

  return app;
}

WebAppRegistryUpdate::AppsToUpdate WebAppRegistryUpdate::TakeAppsToUpdate() {
  AppsToUpdate apps_to_update = std::move(apps_to_update_);
  apps_to_update_.clear();
  return apps_to_update;
}

ScopedRegistryUpdate::ScopedRegistryUpdate(WebAppSyncBridge* sync_bridge)
    : update_(sync_bridge->BeginUpdate()), sync_bridge_(sync_bridge) {}

ScopedRegistryUpdate::~ScopedRegistryUpdate() {
  sync_bridge_->CommitUpdate(std::move(update_), base::DoNothing());
}

}  // namespace web_app
