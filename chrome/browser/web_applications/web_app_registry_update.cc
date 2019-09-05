// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registry_update.h"

#include "base/bind_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

WebAppRegistryUpdate::WebAppRegistryUpdate(WebAppRegistrar* registrar)
    : registrar_(registrar) {}

WebAppRegistryUpdate::~WebAppRegistryUpdate() = default;

WebApp* WebAppRegistryUpdate::UpdateApp(const AppId& app_id) {
  WebApp* app = registrar_->GetAppByIdMutable(app_id);
  if (app)
    apps_to_update_.insert(app);

  return app;
}

ScopedRegistryUpdate::ScopedRegistryUpdate(WebAppRegistrar* registrar)
    : update_(registrar->BeginUpdate()) {}

ScopedRegistryUpdate::~ScopedRegistryUpdate() {
  update_->registrar_->CommitUpdate(std::move(update_), base::DoNothing());
}

}  // namespace web_app
