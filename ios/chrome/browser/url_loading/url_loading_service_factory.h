// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"

class UrlLoadingService;

// Singleton that owns all UrlLoadingServices and associates them with
// ios::ChromeBrowserState.
class UrlLoadingServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static UrlLoadingService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static UrlLoadingServiceFactory* GetInstance();

  // Returns the default factory used to build TestUrlLoadingServices. Can be
  // registered with SetTestingFactory to use test instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<UrlLoadingServiceFactory>;

  UrlLoadingServiceFactory();
  ~UrlLoadingServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(UrlLoadingServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_FACTORY_H_
