// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_notification_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/updates/internal/update_notification_service_impl.h"
#include "chrome/browser/updates/update_notification_config.h"
#include "chrome/browser/updates/update_notification_service_bridge.h"
#include "chrome/browser/updates/update_notification_service_bridge_android.h"
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
  auto* schedule_service =
      NotificationScheduleServiceFactory::GetForBrowserContext(context);
  auto config = updates::UpdateNotificationConfig::Create();
  auto bridge =
      std::make_unique<updates::UpdateNotificationServiceBridgeAndroid>();
  return static_cast<KeyedService*>(new updates::UpdateNotificationServiceImpl(
      schedule_service, std::move(config), std::move(bridge)));
}

content::BrowserContext*
UpdateNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

UpdateNotificationServiceFactory::~UpdateNotificationServiceFactory() = default;
