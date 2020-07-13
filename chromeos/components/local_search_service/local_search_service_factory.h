// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_FACTORY_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace chromeos {

namespace local_search_service {

class LocalSearchService;

class LocalSearchServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LocalSearchService* GetForBrowserContext(
      content::BrowserContext* context);

  static LocalSearchServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LocalSearchServiceFactory>;

  LocalSearchServiceFactory();
  ~LocalSearchServiceFactory() override;

  LocalSearchServiceFactory(const LocalSearchServiceFactory&) = delete;
  LocalSearchServiceFactory& operator=(const LocalSearchServiceFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory overrides.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_FACTORY_H_
