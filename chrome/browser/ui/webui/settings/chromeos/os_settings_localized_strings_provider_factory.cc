// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider_factory.h"

#include "chrome/browser/local_search_service/local_search_service_proxy.h"
#include "chrome/browser/local_search_service/local_search_service_proxy_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace settings {

// static
OsSettingsLocalizedStringsProvider*
OsSettingsLocalizedStringsProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<OsSettingsLocalizedStringsProvider*>(
      OsSettingsLocalizedStringsProviderFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
OsSettingsLocalizedStringsProviderFactory*
OsSettingsLocalizedStringsProviderFactory::GetInstance() {
  return base::Singleton<OsSettingsLocalizedStringsProviderFactory>::get();
}

OsSettingsLocalizedStringsProviderFactory::
    OsSettingsLocalizedStringsProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "OsSettingsLocalizedStringsProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
}

OsSettingsLocalizedStringsProviderFactory::
    ~OsSettingsLocalizedStringsProviderFactory() = default;

KeyedService*
OsSettingsLocalizedStringsProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new OsSettingsLocalizedStringsProvider(
      profile,
      local_search_service::LocalSearchServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(profile))
          ->GetLocalSearchService());
}

bool OsSettingsLocalizedStringsProviderFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

content::BrowserContext*
OsSettingsLocalizedStringsProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace settings
}  // namespace chromeos
