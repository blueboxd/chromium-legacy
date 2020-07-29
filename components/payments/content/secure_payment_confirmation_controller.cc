// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_controller.h"

namespace payments {

SecurePaymentConfirmationController::SecurePaymentConfirmationController() =
    default;

SecurePaymentConfirmationController::~SecurePaymentConfirmationController() =
    default;

void SecurePaymentConfirmationController::OnDismiss() {}

void SecurePaymentConfirmationController::OnCancel() {}

void SecurePaymentConfirmationController::OnConfirm() {}

}  // namespace payments
