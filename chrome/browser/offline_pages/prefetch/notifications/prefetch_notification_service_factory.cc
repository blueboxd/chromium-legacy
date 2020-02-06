// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_factory.h"

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
PrefetchNotificationServiceFactory*
PrefetchNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<PrefetchNotificationServiceFactory> instance;
  return instance.get();
}

// static
offline_pages::PrefetchNotificationService*
PrefetchNotificationServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<offline_pages::PrefetchNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

PrefetchNotificationServiceFactory::PrefetchNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "offline_pages::PrefetchNotificationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

KeyedService* PrefetchNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* schedule_service =
      NotificationScheduleServiceFactory::GetForBrowserContext(context);
  return static_cast<KeyedService*>(
      new offline_pages::PrefetchNotificationServiceImpl(schedule_service));
}

content::BrowserContext*
PrefetchNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

PrefetchNotificationServiceFactory::~PrefetchNotificationServiceFactory() =
    default;
