// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AnnouncementNotificationService;
class Profile;

// Factory to create FastTrackNotificationService.
class AnnouncementNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AnnouncementNotificationServiceFactory* GetInstance();
  static AnnouncementNotificationService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<AnnouncementNotificationServiceFactory>;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  AnnouncementNotificationServiceFactory();
  ~AnnouncementNotificationServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationServiceFactory);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_FACTORY_H_
