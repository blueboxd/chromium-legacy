// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/proxy/local_search_service_proxy_factory.h"

#include "chrome/browser/chromeos/local_search_service/local_search_service_factory.h"
#include "chrome/browser/chromeos/local_search_service/proxy/local_search_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace local_search_service {

// static
LocalSearchServiceProxy* LocalSearchServiceProxyFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LocalSearchServiceProxy*>(
      LocalSearchServiceProxyFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
LocalSearchServiceProxyFactory* LocalSearchServiceProxyFactory::GetInstance() {
  static base::NoDestructor<LocalSearchServiceProxyFactory> instance;
  return instance.get();
}

LocalSearchServiceProxyFactory::LocalSearchServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSearchServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(local_search_service::LocalSearchServiceFactory::GetInstance());
}

LocalSearchServiceProxyFactory::~LocalSearchServiceProxyFactory() = default;

KeyedService* LocalSearchServiceProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = Profile::FromBrowserContext(context);
  return new LocalSearchServiceProxy(
      local_search_service::LocalSearchServiceFactory::GetForProfile(profile));
}

}  // namespace local_search_service
