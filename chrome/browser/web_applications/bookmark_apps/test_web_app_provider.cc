// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/test_web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
std::unique_ptr<KeyedService> TestWebAppProvider::BuildDefault(
    content::BrowserContext* context) {
  return std::make_unique<TestWebAppProvider>(
      Profile::FromBrowserContext(context));
}

TestWebAppProvider::TestWebAppProvider(Profile* profile)
    : WebAppProvider(profile) {}

TestWebAppProvider::~TestWebAppProvider() = default;

void TestWebAppProvider::SetRegistrar(std::unique_ptr<AppRegistrar> registrar) {
  registrar_ = std::move(registrar);
  ConnectSubsystems();
}

void TestWebAppProvider::SetInstallManager(
    std::unique_ptr<InstallManager> install_manager) {
  install_manager_ = std::move(install_manager);
  ConnectSubsystems();
}

void TestWebAppProvider::SetInstallFinalizer(
    std::unique_ptr<InstallFinalizer> install_finalizer) {
  install_finalizer_ = std::move(install_finalizer);
  ConnectSubsystems();
}

void TestWebAppProvider::SetPendingAppManager(
    std::unique_ptr<PendingAppManager> pending_app_manager) {
  pending_app_manager_ = std::move(pending_app_manager);
  ConnectSubsystems();
}

void TestWebAppProvider::SetWebAppUiManager(
    std::unique_ptr<WebAppUiManager> ui_manager) {
  ui_manager_ = std::move(ui_manager);
  ConnectSubsystems();
}

void TestWebAppProvider::SetSystemWebAppManager(
    std::unique_ptr<SystemWebAppManager> system_web_app_manager) {
  system_web_app_manager_ = std::move(system_web_app_manager);
  ConnectSubsystems();
}

void TestWebAppProvider::SetWebAppPolicyManager(
    std::unique_ptr<WebAppPolicyManager> web_app_policy_manager) {
  web_app_policy_manager_ = std::move(web_app_policy_manager);
  ConnectSubsystems();
}

TestWebAppProviderCreator::TestWebAppProviderCreator(
    CreateWebAppProviderCallback callback)
    : callback_(std::move(callback)) {
  will_create_browser_context_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
              base::BindRepeating(&TestWebAppProviderCreator::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

TestWebAppProviderCreator::~TestWebAppProviderCreator() = default;

void TestWebAppProviderCreator::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  WebAppProviderFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(&TestWebAppProviderCreator::CreateWebAppProvider,
                          base::Unretained(this)));
}

std::unique_ptr<KeyedService> TestWebAppProviderCreator::CreateWebAppProvider(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!AreWebAppsEnabled(profile) || !callback_)
    return nullptr;
  return std::move(callback_).Run(profile);
}

}  // namespace web_app
