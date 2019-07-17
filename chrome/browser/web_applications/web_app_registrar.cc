// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"
#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar(Profile* profile,
                                 AbstractWebAppDatabase* database)
    : AppRegistrar(profile), database_(database) {
  DCHECK(database_);
}

WebAppRegistrar::~WebAppRegistrar() = default;

void WebAppRegistrar::RegisterApp(std::unique_ptr<WebApp> web_app) {
  const auto app_id = web_app->app_id();
  DCHECK(!app_id.empty());
  DCHECK(!GetAppById(app_id));

  database_->WriteWebApp(*web_app);

  registry_.emplace(app_id, std::move(web_app));
}

std::unique_ptr<WebApp> WebAppRegistrar::UnregisterApp(const AppId& app_id) {
  DCHECK(!app_id.empty());

  database_->DeleteWebApps({app_id});

  auto kv = registry_.find(app_id);
  DCHECK(kv != registry_.end());

  auto web_app = std::move(kv->second);
  registry_.erase(kv);
  return web_app;
}

WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  auto kv = registry_.find(app_id);
  return kv == registry_.end() ? nullptr : kv->second.get();
}

void WebAppRegistrar::UnregisterAll() {
  std::vector<AppId> app_ids;
  for (auto& kv : registry()) {
    const AppId& app_id = kv.first;
    app_ids.push_back(app_id);
  }
  database_->DeleteWebApps(std::move(app_ids));

  registry_.clear();
}

void WebAppRegistrar::Init(base::OnceClosure callback) {
  database_->OpenDatabase(base::BindOnce(&WebAppRegistrar::OnDatabaseOpened,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback)));
}

void WebAppRegistrar::OnDatabaseOpened(base::OnceClosure callback,
                                       Registry registry) {
  DCHECK(is_empty());
  registry_ = std::move(registry);
  std::move(callback).Run();
}

bool WebAppRegistrar::IsInstalled(const GURL& start_url) const {
  NOTIMPLEMENTED();
  return false;
}

bool WebAppRegistrar::IsInstalled(const AppId& app_id) const {
  return GetAppById(app_id) != nullptr;
}

bool WebAppRegistrar::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

base::Optional<AppId> WebAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

int WebAppRegistrar::CountUserInstalledApps() const {
  NOTIMPLEMENTED();
  return 0;
}

std::string WebAppRegistrar::GetAppShortName(const AppId& app_id) const {
  WebApp* web_app = GetAppById(app_id);
  return web_app ? web_app->name() : std::string();
}

std::string WebAppRegistrar::GetAppDescription(const AppId& app_id) const {
  WebApp* web_app = GetAppById(app_id);
  return web_app ? web_app->description() : std::string();
}

base::Optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  WebApp* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : base::nullopt;
}

const GURL& WebAppRegistrar::GetAppLaunchURL(const AppId& app_id) const {
  WebApp* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_url() : GURL::EmptyGURL();
}

base::Optional<GURL> WebAppRegistrar::GetAppScope(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace web_app
