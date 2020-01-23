// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/updates/announcement_notification/empty_announcement_notification_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
AnnouncementNotificationServiceFactory*
AnnouncementNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<AnnouncementNotificationServiceFactory> instance;
  return instance.get();
}

// static
AnnouncementNotificationService*
AnnouncementNotificationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AnnouncementNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

KeyedService* AnnouncementNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    return new EmptyAnnouncementNotificationService();
  }

  PrefService* pref = Profile::FromBrowserContext(context)->GetPrefs();
  return AnnouncementNotificationService::Create(
      pref, std::make_unique<AnnouncementNotificationDelegate>());
}

content::BrowserContext*
AnnouncementNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

AnnouncementNotificationServiceFactory::AnnouncementNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AnnouncementNotificationService",
          BrowserContextDependencyManager::GetInstance()) {}

AnnouncementNotificationServiceFactory::
    ~AnnouncementNotificationServiceFactory() = default;
