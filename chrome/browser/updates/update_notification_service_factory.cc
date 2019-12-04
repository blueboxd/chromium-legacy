// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_service_factory.h"

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/updates/update_notification_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
UpdateNotificationServiceFactory*
UpdateNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<UpdateNotificationServiceFactory> instance;
  return instance.get();
}

// static
updates::UpdateNotificationService*
UpdateNotificationServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<updates::UpdateNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

UpdateNotificationServiceFactory::UpdateNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "updates::UpdateNotificationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

KeyedService* UpdateNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return static_cast<KeyedService*>(
      new updates::UpdateNotificationServiceImpl());
}

content::BrowserContext*
UpdateNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

UpdateNotificationServiceFactory::~UpdateNotificationServiceFactory() = default;
