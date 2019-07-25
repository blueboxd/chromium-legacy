// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#endif  // OS_CHROMEOS

namespace apps {

// static
AppServiceProxy* AppServiceProxyFactory::GetForProfile(Profile* profile) {
  // TODO: decide the right behaviour in incognito:
  //   - return nullptr (means we need to null check the service at call sites
  //     OR ensure it's never accessed from an incognito profile),
  //   - return the service attached to the Profile that the incognito profile
  //     is branched from (i.e. "inherit" the parent service),
  //   - return a temporary service just for the incognito session (probably
  //     the least sensible option).
  return static_cast<AppServiceProxy*>(
      AppServiceProxyFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
AppServiceProxyFactory* AppServiceProxyFactory::GetInstance() {
  return base::Singleton<AppServiceProxyFactory>::get();
}

// static
bool AppServiceProxyFactory::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kAppServiceAsh) ||
         base::FeatureList::IsEnabled(features::kAppServiceServer) ||
         base::FeatureList::IsEnabled(features::kAppManagement);
}

AppServiceProxyFactory::AppServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "AppServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {
#if defined(OS_CHROMEOS)
  DependsOn(crostini::CrostiniRegistryServiceFactory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
#endif  // OS_CHROMEOS
}

AppServiceProxyFactory::~AppServiceProxyFactory() = default;

KeyedService* AppServiceProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppServiceProxy(Profile::FromBrowserContext(context));
}

bool AppServiceProxyFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
