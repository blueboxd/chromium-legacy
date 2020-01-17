// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_FACTORY_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class BrowserList;

namespace ios {
class ChromeBrowserState;
}

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR browser states.
class BrowserListFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // BrowserList.
  static BrowserList* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  // Getter for singleton instance.
  static BrowserListFactory* GetInstance();

 private:
  friend class base::NoDestructor<BrowserListFactory>;

  BrowserListFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BrowserListFactory);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_FACTORY_H_
