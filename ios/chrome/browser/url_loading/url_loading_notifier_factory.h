// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_FACTORY_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"

class UrlLoadingNotifier;

// Singleton that owns all UrlLoadingNotifiers and associates them with
// ios::ChromeBrowserState.
class UrlLoadingNotifierFactory : public BrowserStateKeyedServiceFactory {
 public:
  static UrlLoadingNotifier* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static UrlLoadingNotifierFactory* GetInstance();

 private:
  friend class base::NoDestructor<UrlLoadingNotifierFactory>;

  UrlLoadingNotifierFactory();
  ~UrlLoadingNotifierFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(UrlLoadingNotifierFactory);
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_FACTORY_H_
