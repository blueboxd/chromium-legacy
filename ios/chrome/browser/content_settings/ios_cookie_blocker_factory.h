// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_IOS_COOKIE_BLOCKER_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_IOS_COOKIE_BLOCKER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class IOSCookieBlocker;

class IOSCookieBlockerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static IOSCookieBlocker* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static IOSCookieBlockerFactory* GetInstance();

  IOSCookieBlockerFactory(const IOSCookieBlockerFactory&) = delete;
  IOSCookieBlockerFactory& operator=(const IOSCookieBlockerFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSCookieBlockerFactory>;

  IOSCookieBlockerFactory();
  ~IOSCookieBlockerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_IOS_COOKIE_BLOCKER_FACTORY_H_
