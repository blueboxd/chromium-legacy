// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace offline_pages {
class PrefetchNotificationService;
}  // namespace offline_pages

class PrefetchNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PrefetchNotificationServiceFactory* GetInstance();
  static offline_pages::PrefetchNotificationService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<PrefetchNotificationServiceFactory>;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  PrefetchNotificationServiceFactory();
  ~PrefetchNotificationServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(PrefetchNotificationServiceFactory);
};

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_FACTORY_H_
