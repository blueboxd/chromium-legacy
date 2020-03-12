// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_bridge_android.h"
#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_impl.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

// static
PrefetchNotificationServiceFactory*
PrefetchNotificationServiceFactory::GetInstance() {
  return base::Singleton<PrefetchNotificationServiceFactory>::get();
}

// static
offline_pages::prefetch::PrefetchNotificationService*
PrefetchNotificationServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<offline_pages::prefetch::PrefetchNotificationService*>(
      GetInstance()->GetServiceForKey(key, true /* create */));
}

PrefetchNotificationServiceFactory::PrefetchNotificationServiceFactory()
    : SimpleKeyedServiceFactory(
          "offline_pages::prefetch::PrefetchNotificationService",
          SimpleDependencyManager::GetInstance()) {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

PrefetchNotificationServiceFactory::~PrefetchNotificationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
PrefetchNotificationServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto bridge = std::make_unique<
      offline_pages::prefetch::PrefetchNotificationServiceBridgeAndroid>();
  auto* schedule_service = NotificationScheduleServiceFactory::GetForKey(key);
  return std::make_unique<
      offline_pages::prefetch::PrefetchNotificationServiceImpl>(
      schedule_service, std::move(bridge));
}

SimpleFactoryKey* PrefetchNotificationServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}
