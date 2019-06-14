// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRAR_H_

#include <set>

#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class TestAppRegistrar : public AppRegistrar {
 public:
  explicit TestAppRegistrar(Profile* profile);
  ~TestAppRegistrar() override;

  // Adds |app_id| to the map of installed apps.
  void AddAsInstalled(const AppId& app_id);

  // Removes |app_id| from the map of installed apps.
  void RemoveAsInstalled(const AppId& app_id);

  // Adds |app_id| to the map of external extensions uninstalled by the user.
  void AddAsExternalAppUninstalledByUser(const AppId& app_id);

  // AppRegistrar
  void Init(base::OnceClosure callback) override;
  bool IsInstalled(const GURL& start_url) const override;
  bool IsInstalled(const AppId& app_id) const override;
  bool WasExternalAppUninstalledByUser(const AppId& app_id) const override;
  bool HasScopeUrl(const AppId& app_id) const override;
  GURL GetScopeUrlForApp(const AppId& app_id) const override;
  AppId FindAppIdForUrl(const GURL& url) const override;

 private:
  std::set<AppId> installed_apps_;
  std::set<AppId> uninstalled_external_apps_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_APP_REGISTRAR_H_
