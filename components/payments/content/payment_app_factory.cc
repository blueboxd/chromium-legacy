// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app_factory.h"

namespace payments {

PaymentAppFactory::PaymentAppFactory(PaymentApp::Type type) : type_(type) {}

PaymentAppFactory::~PaymentAppFactory() = default;

}  // namespace payments
