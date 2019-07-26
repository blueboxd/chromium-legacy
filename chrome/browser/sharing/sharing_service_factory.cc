// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
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
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

SharingServiceFactory::~SharingServiceFactory() = default;

KeyedService* SharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  if (!sync_service)
    return nullptr;

  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  gcm::GCMDriver* gcm_driver = gcm_profile_service->driver();
  instance_id::InstanceIDProfileService* instance_id_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile);
  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetLocalDeviceInfoProvider();

  std::unique_ptr<SharingSyncPreference> sync_prefs =
      std::make_unique<SharingSyncPreference>(profile->GetPrefs());
  std::unique_ptr<VapidKeyManager> vapid_key_manager =
      std::make_unique<VapidKeyManager>(sync_prefs.get());
  std::unique_ptr<SharingDeviceRegistration> sharing_device_registration =
      std::make_unique<SharingDeviceRegistration>(
          sync_prefs.get(), instance_id_service->driver(),
          vapid_key_manager.get(), local_device_info_provider);
  std::unique_ptr<SharingFCMSender> fcm_sender =
      std::make_unique<SharingFCMSender>(gcm_driver, local_device_info_provider,
                                         sync_prefs.get(),
                                         vapid_key_manager.get());
  std::unique_ptr<SharingFCMHandler> fcm_handler =
      std::make_unique<SharingFCMHandler>(gcm_driver, fcm_sender.get());

  return new SharingService(std::move(sync_prefs), std::move(vapid_key_manager),
                            std::move(sharing_device_registration),
                            std::move(fcm_sender), std::move(fcm_handler),
                            gcm_driver, device_info_tracker,
                            local_device_info_provider, sync_service);
}

content::BrowserContext* SharingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Sharing features are disabled in incognito.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
}

// Due to the dependency on SyncService, this cannot be instantiated before
// the profile is fully initialized.
bool SharingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

bool SharingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
