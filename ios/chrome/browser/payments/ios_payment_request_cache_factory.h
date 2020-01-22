// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_REQUEST_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_REQUEST_CACHE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"

namespace payments {
class PaymentRequestCache;

// Ensures that there's only one instance of payments::PaymentRequestCache per
// browser state. Allows the PaymentRequestCache instance to be used in browser
// tests.
class IOSPaymentRequestCacheFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PaymentRequestCache* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static IOSPaymentRequestCacheFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSPaymentRequestCacheFactory>;

  IOSPaymentRequestCacheFactory();
  ~IOSPaymentRequestCacheFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSPaymentRequestCacheFactory);
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_REQUEST_CACHE_FACTORY_H_
