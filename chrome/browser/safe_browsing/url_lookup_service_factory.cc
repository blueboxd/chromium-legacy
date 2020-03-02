// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace safe_browsing {

// static
RealTimeUrlLookupService* RealTimeUrlLookupServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RealTimeUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
RealTimeUrlLookupServiceFactory*
RealTimeUrlLookupServiceFactory::GetInstance() {
  return base::Singleton<RealTimeUrlLookupServiceFactory>::get();
}

RealTimeUrlLookupServiceFactory::RealTimeUrlLookupServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RealTimeUrlLookupService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KeyedService* RealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->safe_browsing_service()->GetURLLoaderFactory());
  // When |url_lookup_service| constructs, |cache_manager| is already
  // constructed. Because |cache_manager| is constructed by |services_delegate|
  // when the profile is created and |url_lookup_service| is constructed
  // when the navigation starts.
  VerdictCacheManager* cache_manager =
      g_browser_process->safe_browsing_service()->GetVerdictCacheManager(
          profile);
  DCHECK(cache_manager);
  return new RealTimeUrlLookupService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      cache_manager, IdentityManagerFactory::GetForProfile(profile),
      profile->GetPrefs(), profile->IsOffTheRecord());
}

}  // namespace safe_browsing
