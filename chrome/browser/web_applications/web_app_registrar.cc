// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar(Profile* profile,
                                 AbstractWebAppDatabase* database)
    : AppRegistrar(profile), database_(database) {
  DCHECK(database_);
}

WebAppRegistrar::~WebAppRegistrar() = default;

void WebAppRegistrar::RegisterApp(std::unique_ptr<WebApp> web_app) {
  CountMutation();

  const auto app_id = web_app->app_id();
  DCHECK(!app_id.empty());
  DCHECK(!GetAppById(app_id));

  database_->WriteWebApp(*web_app);

  registry_.emplace(app_id, std::move(web_app));
}

std::unique_ptr<WebApp> WebAppRegistrar::UnregisterApp(const AppId& app_id) {
  CountMutation();

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
  CountMutation();

  database_->DeleteWebApps(GetAppIds());
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

WebAppRegistrar* WebAppRegistrar::AsWebAppRegistrar() {
  return this;
}

bool WebAppRegistrar::IsInstalled(const AppId& app_id) const {
  return GetAppById(app_id) != nullptr;
}

bool WebAppRegistrar::IsLocallyInstalled(const AppId& app_id) const {
  WebApp* web_app = GetAppById(app_id);
  return web_app ? web_app->is_locally_installed() : false;
}

bool WebAppRegistrar::IsLocallyInstalled(const GURL& start_url) const {
  NOTIMPLEMENTED();
  return false;
}

bool WebAppRegistrar::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return false;
}

bool WebAppRegistrar::WasInstalledByUser(const AppId& app_id) const {
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
  WebApp* web_app = GetAppById(app_id);
  return web_app ? base::Optional<GURL>(web_app->scope()) : base::nullopt;
}

LaunchContainer WebAppRegistrar::GetAppLaunchContainer(
    const AppId& app_id) const {
  WebApp* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_container() : LaunchContainer::kDefault;
}

std::vector<AppId> WebAppRegistrar::GetAppIds() const {
  std::vector<AppId> app_ids;
  app_ids.reserve(registry_.size());

  for (auto& app : AllApps())
    app_ids.push_back(app.app_id());

  return app_ids;
}

WebAppRegistrar::AppSet::Iter::Iter(InternalIter&& internal_iter)
    : internal_iter_(std::move(internal_iter)) {}

WebAppRegistrar::AppSet::Iter::Iter(Iter&&) = default;

WebAppRegistrar::AppSet::Iter::~Iter() = default;

WebAppRegistrar::AppSet::AppSet(const WebAppRegistrar* registrar)
    : begin_(registrar->registry_.begin()),
      end_(registrar->registry_.end())
#if DCHECK_IS_ON()
      ,
      registrar_(registrar),
      mutations_count_(registrar->mutations_count_)
#endif
{
}

WebAppRegistrar::AppSet::~AppSet() {
#if DCHECK_IS_ON()
  DCHECK_EQ(mutations_count_, registrar_->mutations_count_);
#endif
}

WebAppRegistrar::AppSet WebAppRegistrar::AllApps() const {
  return AppSet(this);
}

void WebAppRegistrar::CountMutation() {
#if DCHECK_IS_ON()
  ++mutations_count_;
#endif
}

}  // namespace web_app
