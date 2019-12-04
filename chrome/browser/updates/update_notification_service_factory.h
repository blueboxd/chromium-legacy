// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace updates {
class UpdateNotificationService;
}  // namespace updates

class UpdateNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static UpdateNotificationServiceFactory* GetInstance();
  static updates::UpdateNotificationService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<UpdateNotificationServiceFactory>;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  UpdateNotificationServiceFactory();
  ~UpdateNotificationServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationServiceFactory);
};

#endif  // CHROME_BROWSER_UPDATES_UPDATE_NOTIFICATION_SERVICE_FACTORY_H_
