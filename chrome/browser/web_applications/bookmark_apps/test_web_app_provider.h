// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_TEST_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_TEST_WEB_APP_PROVIDER_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_list.h"
#include "chrome/browser/web_applications/web_app_provider.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace web_app {

class AppRegistrar;
class InstallManager;
class InstallFinalizer;
class PendingAppManager;
class SystemWebAppManager;
class WebAppPolicyManager;

class TestWebAppProvider : public WebAppProvider {
 public:
  // Builds a default WebAppProvider that won't be started. To activate this
  // default instance, call WebAppProvider::StartRegistry().
  static std::unique_ptr<KeyedService> BuildDefault(
      content::BrowserContext* context);

  explicit TestWebAppProvider(Profile* profile);
  ~TestWebAppProvider() override;

  void SetRegistrar(std::unique_ptr<AppRegistrar> registrar);
  void SetInstallManager(std::unique_ptr<InstallManager> install_manager);
  void SetInstallFinalizer(std::unique_ptr<InstallFinalizer> install_finalizer);
  void SetPendingAppManager(
      std::unique_ptr<PendingAppManager> pending_app_manager);
  void SetWebAppUiManager(std::unique_ptr<WebAppUiManager> ui_manager);
  void SetSystemWebAppManager(
      std::unique_ptr<SystemWebAppManager> system_web_app_manager);
  void SetWebAppPolicyManager(
      std::unique_ptr<WebAppPolicyManager> web_app_policy_manager);
};

class TestWebAppProviderCreator {
 public:
  using CreateWebAppProviderCallback =
      base::OnceCallback<std::unique_ptr<KeyedService>(Profile* profile)>;

  explicit TestWebAppProviderCreator(CreateWebAppProviderCallback callback);
  ~TestWebAppProviderCreator();

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::unique_ptr<KeyedService> CreateWebAppProvider(
      content::BrowserContext* context);

  CreateWebAppProviderCallback callback_;

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_BOOKMARK_APPS_TEST_WEB_APP_PROVIDER_H_
