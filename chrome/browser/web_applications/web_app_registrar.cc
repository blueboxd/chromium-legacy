// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/web_applications/abstract_web_app_sync_bridge.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar(Profile* profile,
                                 AbstractWebAppSyncBridge* sync_bridge)
    : AppRegistrar(profile), sync_bridge_(sync_bridge) {
  DCHECK(sync_bridge_);
}

WebAppRegistrar::~WebAppRegistrar() = default;

void WebAppRegistrar::RegisterApp(std::unique_ptr<WebApp> web_app) {
  CountMutation();

  const auto app_id = web_app->app_id();
  DCHECK(!app_id.empty());
  DCHECK(!GetAppById(app_id));

  // TODO(loyso): Expose CompletionCallback as RegisterApp argument.
  sync_bridge_->WriteWebApps({web_app.get()}, base::DoNothing());

  registry_.emplace(app_id, std::move(web_app));
}

std::unique_ptr<WebApp> WebAppRegistrar::UnregisterApp(const AppId& app_id) {
  CountMutation();

  DCHECK(!app_id.empty());

  // TODO(loyso): Expose CompletionCallback as UnregisterApp argument.
  sync_bridge_->DeleteWebApps({app_id}, base::DoNothing());

  auto kv = registry_.find(app_id);
  DCHECK(kv != registry_.end());

  auto web_app = std::move(kv->second);
  registry_.erase(kv);
  return web_app;
}

void WebAppRegistrar::UnregisterAll() {
  CountMutation();

  // TODO(loyso): Expose CompletionCallback as UnregisterAll argument.
  sync_bridge_->DeleteWebApps(GetAppIds(), base::DoNothing());
  registry_.clear();
}

const WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  auto it = registry_.find(app_id);
  return it == registry_.end() ? nullptr : it->second.get();
}

std::unique_ptr<WebAppRegistryUpdate> WebAppRegistrar::BeginUpdate() {
  DCHECK(!is_in_update_);
  is_in_update_ = true;

  return base::WrapUnique(new WebAppRegistryUpdate(this));
}

void WebAppRegistrar::CommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update,
                                   CommitCallback callback) {
  DCHECK(is_in_update_);
  is_in_update_ = false;
  if (update == nullptr || update->apps_to_update_.empty()) {
    std::move(callback).Run(/*success*/ true);
    return;
  }

#if DCHECK_IS_ON()
  for (auto* app : update->apps_to_update_)
    DCHECK(GetAppById(app->app_id()));
#endif

  sync_bridge_->WriteWebApps(
      std::move(update->apps_to_update_),
      base::BindOnce(&WebAppRegistrar::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppRegistrar::Init(base::OnceClosure callback) {
  sync_bridge_->OpenDatabase(base::BindOnce(&WebAppRegistrar::OnDatabaseOpened,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(callback)));
}

void WebAppRegistrar::OnDatabaseOpened(base::OnceClosure callback,
                                       Registry registry) {
  DCHECK(is_empty());
  registry_ = std::move(registry);
  std::move(callback).Run();
}

void WebAppRegistrar::OnDataWritten(CommitCallback callback, bool success) {
  if (!success)
    DLOG(ERROR) << "WebAppRegistrar commit failed";

  std::move(callback).Run(success);
}

WebAppRegistrar* WebAppRegistrar::AsWebAppRegistrar() {
  return this;
}

bool WebAppRegistrar::IsInstalled(const AppId& app_id) const {
  return GetAppById(app_id) != nullptr;
}

bool WebAppRegistrar::IsLocallyInstalled(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->is_locally_installed() : false;
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
  const std::string url_path = url.spec();

  base::Optional<AppId> best_app;
  size_t best_path_length = 0U;
  bool best_is_shortcut = true;

  for (const WebApp& app : AllApps()) {
    // TODO(crbug.com/915038): Implement and use WebApp::IsShortcut().
    // TODO(crbug.com/910016): Treat shortcuts as PWAs.
    bool app_is_shortcut = app.scope().is_empty();
    if (app_is_shortcut && !best_is_shortcut)
      continue;

    const std::string app_path = app.scope().is_empty()
                                     ? app.launch_url().Resolve(".").spec()
                                     : app.scope().spec();
    if ((app_path.size() > best_path_length ||
         (best_is_shortcut && !app_is_shortcut)) &&
        base::StartsWith(url_path, app_path, base::CompareCase::SENSITIVE)) {
      best_app = app.app_id();
      best_path_length = app_path.size();
      best_is_shortcut = app_is_shortcut;
    }
  }

  return best_app;
}

int WebAppRegistrar::CountUserInstalledApps() const {
  NOTIMPLEMENTED();
  return 0;
}

std::string WebAppRegistrar::GetAppShortName(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->name() : std::string();
}

std::string WebAppRegistrar::GetAppDescription(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->description() : std::string();
}

base::Optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : base::nullopt;
}

const GURL& WebAppRegistrar::GetAppLaunchURL(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_url() : GURL::EmptyGURL();
}

base::Optional<GURL> WebAppRegistrar::GetAppScope(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? base::Optional<GURL>(web_app->scope()) : base::nullopt;
}

LaunchContainer WebAppRegistrar::GetAppLaunchContainer(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_container() : LaunchContainer::kDefault;
}

void WebAppRegistrar::SetAppLaunchContainer(const AppId& app_id,
                                            LaunchContainer launch_container) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetLaunchContainer(launch_container);
}

std::vector<AppId> WebAppRegistrar::GetAppIds() const {
  std::vector<AppId> app_ids;
  app_ids.reserve(registry_.size());

  for (const WebApp& app : AllApps())
    app_ids.push_back(app.app_id());

  return app_ids;
}

WebAppRegistrar::AppSet::AppSet(const WebAppRegistrar* registrar)
    : registrar_(registrar)
#if DCHECK_IS_ON()
      ,
      mutations_count_(registrar->mutations_count_)
#endif
{
}

WebAppRegistrar::AppSet::~AppSet() {
#if DCHECK_IS_ON()
  DCHECK_EQ(mutations_count_, registrar_->mutations_count_);
#endif
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::begin() {
  return iterator(registrar_->registry_.begin());
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::end() {
  return iterator(registrar_->registry_.end());
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::begin() const {
  return const_iterator(registrar_->registry_.begin());
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::end() const {
  return const_iterator(registrar_->registry_.end());
}

const WebAppRegistrar::AppSet WebAppRegistrar::AllApps() const {
  return AppSet(this);
}

WebApp* WebAppRegistrar::GetAppByIdMutable(const AppId& app_id) {
  return const_cast<WebApp*>(GetAppById(app_id));
}

WebAppRegistrar::AppSet WebAppRegistrar::AllAppsMutable() {
  return AppSet(this);
}

void WebAppRegistrar::CountMutation() {
#if DCHECK_IS_ON()
  ++mutations_count_;
#endif
}

}  // namespace web_app
