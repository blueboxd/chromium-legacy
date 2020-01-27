// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_keyed_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_keyed_service.h"
#include "ios/web/public/browser_state.h"

// static
BreadcrumbPersistentStorageKeyedServiceFactory*
BreadcrumbPersistentStorageKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<BreadcrumbPersistentStorageKeyedServiceFactory>
      instance;
  return instance.get();
}

// static
BreadcrumbPersistentStorageKeyedService*
BreadcrumbPersistentStorageKeyedServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<BreadcrumbPersistentStorageKeyedService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

BreadcrumbPersistentStorageKeyedServiceFactory::
    BreadcrumbPersistentStorageKeyedServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BreadcrumbPersistentStorageService",
          BrowserStateDependencyManager::GetInstance()) {}

BreadcrumbPersistentStorageKeyedServiceFactory::
    ~BreadcrumbPersistentStorageKeyedServiceFactory() {}

std::unique_ptr<KeyedService>
BreadcrumbPersistentStorageKeyedServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<BreadcrumbPersistentStorageKeyedService>(context);
}

web::BrowserState*
BreadcrumbPersistentStorageKeyedServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Create the service for both normal and incognito browser states.
  return context;
}
