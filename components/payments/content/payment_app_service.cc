// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app_service.h"

#include "base/feature_list.h"
#include "components/payments/content/autofill_payment_app_factory.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/core/payment_app.h"
#include "content/public/common/content_features.h"

namespace payments {

PaymentAppService::PaymentAppService() {
  factories_.emplace_back(std::make_unique<AutofillPaymentAppFactory>());

  if (base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps))
    factories_.emplace_back(std::make_unique<ServiceWorkerPaymentAppFactory>());
}

PaymentAppService::~PaymentAppService() = default;

void PaymentAppService::Create(
    base::WeakPtr<PaymentAppFactory::Delegate> delegate,
    size_t* number_of_payment_app_factories) {
  *number_of_payment_app_factories = factories_.size();
  for (const auto& factory : factories_) {
    factory->Create(delegate);
  }
}

}  // namespace payments
