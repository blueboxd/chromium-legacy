// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class BreadcrumbPersistentStorageKeyedService;
class ChromeBrowserState;

class BreadcrumbPersistentStorageKeyedServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static BreadcrumbPersistentStorageKeyedServiceFactory* GetInstance();
  static BreadcrumbPersistentStorageKeyedService* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<
      BreadcrumbPersistentStorageKeyedServiceFactory>;

  BreadcrumbPersistentStorageKeyedServiceFactory();
  ~BreadcrumbPersistentStorageKeyedServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  BreadcrumbPersistentStorageKeyedServiceFactory(
      const BreadcrumbPersistentStorageKeyedServiceFactory&) = delete;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_FACTORY_H_
