// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;

class WebAppRegistrar : public AppRegistrar {
 public:
  explicit WebAppRegistrar(Profile* profile, AbstractWebAppDatabase* database);
  ~WebAppRegistrar() override;

  void RegisterApp(std::unique_ptr<WebApp> web_app);
  std::unique_ptr<WebApp> UnregisterApp(const AppId& app_id);

  WebApp* GetAppById(const AppId& app_id) const;

  const Registry& registry() const { return registry_; }

  bool is_empty() const { return registry_.empty(); }

  // Clears registry.
  void UnregisterAll();

  // AppRegistrar:
  void Init(base::OnceClosure callback) override;
  bool IsInstalled(const GURL& start_url) const override;
  bool IsInstalled(const AppId& app_id) const override;
  bool WasExternalAppUninstalledByUser(const AppId& app_id) const override;
  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url) const override;
  int CountUserInstalledApps() const override;
  std::string GetAppShortName(const AppId& app_id) const override;
  std::string GetAppDescription(const AppId& app_id) const override;
  base::Optional<SkColor> GetAppThemeColor(const AppId& app_id) const override;
  const GURL& GetAppLaunchURL(const AppId& app_id) const override;
  base::Optional<GURL> GetAppScope(const AppId& app_id) const override;

 private:
  void OnDatabaseOpened(base::OnceClosure callback, Registry registry);

  Registry registry_;

  // An abstract database, not owned by this registrar.
  AbstractWebAppDatabase* database_;

  base::WeakPtrFactory<WebAppRegistrar> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppRegistrar);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
