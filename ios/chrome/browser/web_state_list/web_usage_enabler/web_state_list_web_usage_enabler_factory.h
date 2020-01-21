// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"

class WebStateListWebUsageEnabler;

// WebStateListWebUsageEnablerFactory attaches
// WebStateListWebUsageEnablers to ChromeBrowserStates.
class WebStateListWebUsageEnablerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // WebStateListWebUsageEnabler.
  static WebStateListWebUsageEnabler* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  // Getter for singleton instance.
  static WebStateListWebUsageEnablerFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebStateListWebUsageEnablerFactory>;

  WebStateListWebUsageEnablerFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebStateListWebUsageEnablerFactory);
};
#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_FACTORY_H_
