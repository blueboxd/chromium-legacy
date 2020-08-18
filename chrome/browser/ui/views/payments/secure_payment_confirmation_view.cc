// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_view.h"

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

namespace payments {

// static
base::WeakPtr<SecurePaymentConfirmationView>
SecurePaymentConfirmationView::Create() {
  return (new SecurePaymentConfirmationDialogView(
              /*observer_for_test=*/nullptr))
      ->GetWeakPtr();
}

SecurePaymentConfirmationView::SecurePaymentConfirmationView() = default;
SecurePaymentConfirmationView::~SecurePaymentConfirmationView() = default;

}  // namespace payments
