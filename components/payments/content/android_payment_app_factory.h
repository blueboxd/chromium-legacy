// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_FACTORY_H_

#include "components/payments/content/payment_app_factory.h"

namespace payments {

// Retrieves Android payment apps.
class AndroidPaymentAppFactory : public PaymentAppFactory {
 public:
  AndroidPaymentAppFactory();
  ~AndroidPaymentAppFactory() override;

  AndroidPaymentAppFactory(const AndroidPaymentAppFactory& other) = delete;
  AndroidPaymentAppFactory& operator=(const AndroidPaymentAppFactory& other) =
      delete;

  // PaymentAppFactory:
  void Create(base::WeakPtr<Delegate> delegate) override;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_FACTORY_H_
