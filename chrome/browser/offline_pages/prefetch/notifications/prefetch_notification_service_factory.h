// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace offline_pages {
namespace prefetch {
class PrefetchNotificationService;
}  // namespace prefetch
}  // namespace offline_pages

class PrefetchNotificationServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static PrefetchNotificationServiceFactory* GetInstance();
  static offline_pages::prefetch::PrefetchNotificationService* GetForKey(
      SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<
      PrefetchNotificationServiceFactory>;

  PrefetchNotificationServiceFactory();
  ~PrefetchNotificationServiceFactory() override;

  // SimpleKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(PrefetchNotificationServiceFactory);
};

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_NOTIFICATIONS_PREFETCH_NOTIFICATION_SERVICE_FACTORY_H_
