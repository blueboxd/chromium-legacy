// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace local_search_service {

class LocalSearchServiceProxy;

class LocalSearchServiceProxyFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static LocalSearchServiceProxy* GetForProfile(Profile* profile);
  static LocalSearchServiceProxyFactory* GetInstance();

  LocalSearchServiceProxyFactory(const LocalSearchServiceProxyFactory&) =
      delete;
  LocalSearchServiceProxyFactory& operator=(
      const LocalSearchServiceProxyFactory&) = delete;

 private:
  friend class base::NoDestructor<LocalSearchServiceProxyFactory>;

  LocalSearchServiceProxyFactory();
  ~LocalSearchServiceProxyFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_
