// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_registrar.h"

#include "base/callback.h"
#include "base/stl_util.h"
#include "url/gurl.h"

namespace web_app {

TestAppRegistrar::TestAppRegistrar() : AppRegistrar(nullptr) {}

TestAppRegistrar::~TestAppRegistrar() = default;

void TestAppRegistrar::AddExternalApp(const AppId& app_id,
                                      const AppInfo& info) {
  installed_apps_[app_id] = info;
}

void TestAppRegistrar::RemoveExternalApp(const AppId& app_id) {
  DCHECK(base::Contains(installed_apps_, app_id));
  installed_apps_.erase(app_id);
}

void TestAppRegistrar::RemoveExternalAppByInstallUrl(const GURL& install_url) {
  RemoveExternalApp(*LookupExternalAppId(install_url));
}

void TestAppRegistrar::SimulateExternalAppUninstalledByUser(
    const AppId& app_id) {
  DCHECK(!base::Contains(user_uninstalled_external_apps_, app_id));
  user_uninstalled_external_apps_.insert(app_id);
  if (base::Contains(installed_apps_, app_id))
    RemoveExternalApp(app_id);
}

void TestAppRegistrar::Init(base::OnceClosure callback) {}

bool TestAppRegistrar::IsInstalled(const GURL& start_url) const {
  NOTIMPLEMENTED();
  return false;
}

bool TestAppRegistrar::IsInstalled(const AppId& app_id) const {
  return base::Contains(installed_apps_, app_id);
}

bool TestAppRegistrar::WasExternalAppUninstalledByUser(
    const AppId& app_id) const {
  return base::Contains(user_uninstalled_external_apps_, app_id);
}

std::map<AppId, GURL> TestAppRegistrar::GetExternallyInstalledApps(
    ExternalInstallSource install_source) const {
  std::map<AppId, GURL> apps;
  for (auto& id_and_info : installed_apps_) {
    if (id_and_info.second.source == install_source)
      apps[id_and_info.first] = id_and_info.second.install_url;
  }

  return apps;
}

base::Optional<AppId> TestAppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  auto it = std::find_if(installed_apps_.begin(), installed_apps_.end(),
                         [install_url](const auto& app_it) {
                           return app_it.second.install_url == install_url;
                         });
  return it == installed_apps_.end() ? base::Optional<AppId>() : it->first;
}

bool TestAppRegistrar::HasExternalAppWithInstallSource(
    const AppId& app_id,
    ExternalInstallSource install_source) const {
  auto it = std::find_if(installed_apps_.begin(), installed_apps_.end(),
                         [app_id, install_source](const auto& app_it) {
                           return app_it.first == app_id &&
                                  app_it.second.source == install_source;
                         });
  return it != installed_apps_.end();
}

base::Optional<AppId> TestAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

int TestAppRegistrar::CountUserInstalledApps() const {
  NOTIMPLEMENTED();
  return 0;
}

std::string TestAppRegistrar::GetAppShortName(const AppId&) const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string TestAppRegistrar::GetAppDescription(const AppId&) const {
  NOTIMPLEMENTED();
  return std::string();
}

base::Optional<SkColor> TestAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

const GURL& TestAppRegistrar::GetAppLaunchURL(const AppId&) const {
  NOTIMPLEMENTED();
  return GURL::EmptyGURL();
}

base::Optional<GURL> TestAppRegistrar::GetAppScope(const AppId& app_id) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace web_app
