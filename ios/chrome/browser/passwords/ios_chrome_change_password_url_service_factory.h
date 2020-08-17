// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_CHANGE_PASSWORD_URL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_CHANGE_PASSWORD_URL_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"

class ChromeBrowserState;
enum class ServiceAccessType;

namespace password_manager {
class ChangePasswordUrlService;
}

// Singleton that owns all ChangePasswordUrlServices and associates them with
// ChromeBrowserState.
class IOSChromeChangePasswordUrlServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromeChangePasswordUrlServiceFactory* GetInstance();
  static password_manager::ChangePasswordUrlService* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromeChangePasswordUrlServiceFactory>;

  IOSChromeChangePasswordUrlServiceFactory();
  ~IOSChromeChangePasswordUrlServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_CHANGE_PASSWORD_URL_SERVICE_FACTORY_H_
