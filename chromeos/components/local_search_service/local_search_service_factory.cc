// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_factory.h"

#include "chromeos/components/local_search_service/local_search_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace local_search_service {

LocalSearchService* LocalSearchServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalSearchService*>(
      LocalSearchServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context, true /* create */));
}

LocalSearchServiceFactory* LocalSearchServiceFactory::GetInstance() {
  return base::Singleton<LocalSearchServiceFactory>::get();
}

LocalSearchServiceFactory::LocalSearchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSearchService",
          BrowserContextDependencyManager::GetInstance()) {}

LocalSearchServiceFactory::~LocalSearchServiceFactory() = default;

content::BrowserContext* LocalSearchServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

KeyedService* LocalSearchServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* /* context */) const {
  return new LocalSearchService();
}

}  // namespace local_search_service
}  // namespace chromeos
