// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"

struct WebApplicationInfo;

namespace web_app {

class WebApp;
class WebAppIconManager;
class WebAppRegistrar;

class WebAppInstallFinalizer final : public InstallFinalizer {
 public:
  explicit WebAppInstallFinalizer(WebAppIconManager* icon_manager);
  ~WebAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void UninstallExternalWebApp(
      const GURL& app_url,
      UninstallExternalWebAppCallback callback) override;
  bool CanCreateOsShortcuts() const override;
  void CreateOsShortcuts(const AppId& app_id,
                         bool add_to_desktop,
                         CreateOsShortcutsCallback callback) override;
  bool CanRevealAppShim() const override;
  void RevealAppShim(const AppId& app_id) override;
  bool CanSkipAppUpdateForSync(
      const AppId& app_id,
      const WebApplicationInfo& web_app_info) const override;
  bool CanUserUninstallFromSync(const AppId& app_id) const override;
  void SetSubsystems(AppRegistrar* registrar,
                     WebAppUiManager* ui_manager) override;

 private:
  void OnDataWritten(InstallFinalizedCallback callback,
                     std::unique_ptr<WebApp> web_app,
                     bool success);

  WebAppRegistrar* registrar_ = nullptr;
  WebAppIconManager* icon_manager_;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallFinalizer);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
