// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/web_applications/web_app.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

WebAppRegistrar::WebAppRegistrar(Profile* profile) : AppRegistrar(profile) {}

WebAppRegistrar::~WebAppRegistrar() = default;

const WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  auto it = registry_.find(app_id);
  return it == registry_.end() ? nullptr : it->second.get();
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

blink::mojom::DisplayMode WebAppRegistrar::GetAppDisplayMode(
    const web_app::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode()
                 : blink::mojom::DisplayMode::kUndefined;
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

void WebAppRegistrar::SetRegistry(Registry&& registry) {
  registry_ = std::move(registry);
}

void WebAppRegistrar::CountMutation() {
#if DCHECK_IS_ON()
  ++mutations_count_;
#endif
}

WebAppRegistrarMutable::WebAppRegistrarMutable(Profile* profile)
    : WebAppRegistrar(profile) {}

WebAppRegistrarMutable::~WebAppRegistrarMutable() = default;

void WebAppRegistrarMutable::InitRegistry(Registry&& registry) {
  DCHECK(is_empty());
  SetRegistry(std::move(registry));
}

WebApp* WebAppRegistrarMutable::GetAppByIdMutable(const AppId& app_id) {
  return const_cast<WebApp*>(GetAppById(app_id));
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::AllAppsMutable() {
  return AppSet(this);
}

bool IsRegistryEqual(const Registry& registry, const Registry& registry2) {
  if (registry.size() != registry2.size())
    return false;

  for (auto& kv : registry) {
    const WebApp* web_app = kv.second.get();
    const WebApp* web_app2 = registry2.at(web_app->app_id()).get();
    if (*web_app != *web_app2)
      return false;
  }

  return true;
}

}  // namespace web_app
