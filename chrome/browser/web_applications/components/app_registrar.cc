// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_registrar.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"

namespace web_app {

AppRegistrar::AppRegistrar(Profile* profile) : profile_(profile) {}

AppRegistrar::~AppRegistrar() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnAppRegistrarDestroyed();
}

bool AppRegistrar::IsLocallyInstalled(const GURL& start_url) const {
  return IsLocallyInstalled(GenerateAppIdFromURL(start_url));
}

void AppRegistrar::AddObserver(AppRegistrarObserver* observer) {
  observers_.AddObserver(observer);
}

void AppRegistrar::RemoveObserver(AppRegistrarObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppRegistrar::NotifyWebAppInstalled(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppInstalled(app_id);
  // TODO(alancutter): Call RecordWebAppInstallation here when we get access to
  // the WebappInstallSource in this event.
}

void AppRegistrar::NotifyWebAppUninstalled(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppUninstalled(app_id);
  RecordWebAppUninstallation(profile()->GetPrefs(), app_id);
}

void AppRegistrar::NotifyWebAppProfileWillBeDeleted(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppProfileWillBeDeleted(app_id);
}

void AppRegistrar::NotifyAppRegistrarShutdown() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnAppRegistrarShutdown();
}

std::map<AppId, GURL> AppRegistrar::GetExternallyInstalledApps(
    ExternalInstallSource install_source) const {
  std::map<AppId, GURL> installed_apps =
      ExternallyInstalledWebAppPrefs::BuildAppIdsMap(profile()->GetPrefs(),
                                                     install_source);
  base::EraseIf(installed_apps, [this](const std::pair<AppId, GURL>& app) {
    return !IsInstalled(app.first);
  });

  return installed_apps;
}

base::Optional<AppId> AppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  return ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
      .LookupAppId(install_url);
}

bool AppRegistrar::HasExternalAppWithInstallSource(
    const AppId& app_id,
    ExternalInstallSource install_source) const {
  return ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
      profile()->GetPrefs(), app_id, install_source);
}

std::vector<web_app::AppId> AppRegistrar::FindAppsInScope(
    const GURL& scope) const {
  std::string scope_str = scope.spec();

  std::vector<web_app::AppId> in_scope;
  for (const auto& app_id : GetAppIds()) {
    const base::Optional<GURL>& app_scope = GetAppScope(app_id);
    if (!app_scope)
      continue;

    if (!base::StartsWith(app_scope->spec(), scope_str,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }

    in_scope.push_back(app_id);
  }

  return in_scope;
}

bool AppRegistrar::IsShortcutApp(const AppId& app_id) const {
  // TODO (crbug/910016): Make app scope always return a value and record this
  //  distinction in some other way.
  return !GetAppScope(app_id).has_value();
}

}  // namespace web_app
