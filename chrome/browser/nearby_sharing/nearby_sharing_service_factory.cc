// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr char kServiceName[] = "NearbySharingService";

}  // namespace

// static
NearbySharingServiceFactory* NearbySharingServiceFactory::GetInstance() {
  return base::Singleton<NearbySharingServiceFactory>::get();
}

// static
NearbySharingService* NearbySharingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<NearbySharingServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

NearbySharingServiceFactory::NearbySharingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          kServiceName,
          BrowserContextDependencyManager::GetInstance()) {}

NearbySharingServiceFactory::~NearbySharingServiceFactory() = default;

KeyedService* NearbySharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kNearbySharing)) {
    VLOG(1) << __func__ << ": Nearby Sharing feature flag is not enabled.";
    return nullptr;
  }

  PrefService* pref_service = Profile::FromBrowserContext(context)->GetPrefs();

  if (!pref_service->GetBoolean(kNearbySharingEnabledPrefName)) {
    VLOG(1) << __func__ << ": Nearby Sharing feature not enabled in prefs.";
    return nullptr;
  }

  VLOG(1) << __func__ << ": creating NearbySharingService.";
  return new NearbySharingServiceImpl(Profile::FromBrowserContext(context));
}

content::BrowserContext* NearbySharingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Nearby Sharing features are disabled in incognito.
  if (context->IsOffTheRecord())
    return nullptr;

  // TODO(vecore): Ensure only one profile gets bound to Nearby Sharing.
  return context;
}

void NearbySharingServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterNearbySharingPrefs(registry);
}

bool NearbySharingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool NearbySharingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
