// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chromeos/constants/chromeos_features.h"
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
  DependsOn(local_search_service::LocalSearchServiceFactory::GetInstance());
  DependsOn(OsSettingsManagerFactory::GetInstance());
}

SearchHandlerFactory::~SearchHandlerFactory() = default;

KeyedService* SearchHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kNewOsSettingsSearch))
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  return new SearchHandler(
      OsSettingsManagerFactory::GetForProfile(profile),
      local_search_service::LocalSearchServiceFactory::GetForProfile(
          Profile::FromBrowserContext(profile)));
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
