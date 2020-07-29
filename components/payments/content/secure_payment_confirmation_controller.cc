// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/payments/content/payment_request.h"

namespace payments {

SecurePaymentConfirmationController::SecurePaymentConfirmationController() =
    default;

SecurePaymentConfirmationController::~SecurePaymentConfirmationController() =
    default;

void SecurePaymentConfirmationController::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PaymentRequest::UserCancelled, request));
}

void SecurePaymentConfirmationController::CloseDialog() {}

void SecurePaymentConfirmationController::ShowProcessingSpinner() {}

void SecurePaymentConfirmationController::OnDismiss() {}

void SecurePaymentConfirmationController::OnCancel() {}

void SecurePaymentConfirmationController::OnConfirm() {}

}  // namespace payments
