// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/core/method_strings.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/origin.h"

namespace payments {

SecurePaymentConfirmationAppFactory::SecurePaymentConfirmationAppFactory()
    : PaymentAppFactory(PaymentApp::Type::INTERNAL) {}

SecurePaymentConfirmationAppFactory::~SecurePaymentConfirmationAppFactory() =
    default;

void SecurePaymentConfirmationAppFactory::Create(
    base::WeakPtr<Delegate> delegate) {
  PaymentRequestSpec* spec = delegate->GetSpec();
  if (!base::Contains(spec->payment_method_identifiers_set(),
                      methods::kSecurePaymentConfirmation)) {
    delegate->OnDoneCreatingPaymentApps();
    return;
  }

  for (const mojom::PaymentMethodDataPtr& method_data : spec->method_data()) {
    if (method_data->supported_method == methods::kSecurePaymentConfirmation &&
        method_data->secure_payment_confirmation) {
      // TODO(https://crbug.com/1110324): Check storage for whether
      // |method_data->secure_payment_confirmation->instrument_id| has any
      // credentials on this device. If so, retrieve the instrument icon and
      // label from storage and use these values to create a
      // SecurePaymentConfirmationApp.

      // A stub payment app that contains the secure payment confirmation
      // request, but does not yet invoke WebAuthn at this time.
      delegate->OnPaymentAppCreated(
          std::make_unique<SecurePaymentConfirmationApp>(
              /*icon=*/nullptr, /*label=*/base::ASCIIToUTF16("Stub label"),
              /*merchant_origin=*/url::Origin::Create(delegate->GetTopOrigin()),
              /*total=*/spec->details().total->amount,
              /*request=*/method_data->secure_payment_confirmation));
      break;
    }
  }

  delegate->OnDoneCreatingPaymentApps();
}

}  // namespace payments
