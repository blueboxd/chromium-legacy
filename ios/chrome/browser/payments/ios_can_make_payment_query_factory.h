// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_IOS_CAN_MAKE_PAYMENT_QUERY_FACTORY_H_
#define IOS_CHROME_BROWSER_PAYMENTS_IOS_CAN_MAKE_PAYMENT_QUERY_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace payments {
class CanMakePaymentQuery;
}  // namespace payments

// Ensures that there's only one instance of CanMakePaymentQuery per browser
// state.
class IOSCanMakePaymentQueryFactory : public BrowserStateKeyedServiceFactory {
 public:
  static IOSCanMakePaymentQueryFactory* GetInstance();
  static payments::CanMakePaymentQuery* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSCanMakePaymentQueryFactory>;

  IOSCanMakePaymentQueryFactory();
  ~IOSCanMakePaymentQueryFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSCanMakePaymentQueryFactory);
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_IOS_CAN_MAKE_PAYMENT_QUERY_FACTORY_H_
