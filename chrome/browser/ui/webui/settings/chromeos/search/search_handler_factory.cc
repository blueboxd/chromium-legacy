// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler_factory.h"

#include "chrome/browser/local_search_service/local_search_service_proxy.h"
#include "chrome/browser/local_search_service/local_search_service_proxy_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace settings {

// static
SearchHandler* SearchHandlerFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchHandler*>(
      SearchHandlerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
SearchHandlerFactory* SearchHandlerFactory::GetInstance() {
  return base::Singleton<SearchHandlerFactory>::get();
}

SearchHandlerFactory::SearchHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "SearchHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
  DependsOn(OsSettingsLocalizedStringsProviderFactory::GetInstance());
}

SearchHandlerFactory::~SearchHandlerFactory() = default;

KeyedService* SearchHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SearchHandler(
      OsSettingsLocalizedStringsProviderFactory::GetForProfile(profile),
      local_search_service::LocalSearchServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(profile))
          ->GetLocalSearchServiceImpl());
}

bool SearchHandlerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

content::BrowserContext* SearchHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace settings
}  // namespace chromeos
