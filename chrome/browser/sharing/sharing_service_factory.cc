// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "content/public/browser/browser_context.h"

namespace {
constexpr char kServiceName[] = "SharingService";
}  // namespace

// static
SharingServiceFactory* SharingServiceFactory::GetInstance() {
  return base::Singleton<SharingServiceFactory>::get();
}

// static
SharingService* SharingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SharingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SharingServiceFactory::SharingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          kServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

SharingServiceFactory::~SharingServiceFactory() = default;

KeyedService* SharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();

  std::unique_ptr<SharingSyncPreference> sync_prefs =
      std::make_unique<SharingSyncPreference>(profile->GetPrefs());

  return new SharingService(std::move(sync_prefs), device_info_tracker);
}

content::BrowserContext* SharingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SharingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SharingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
