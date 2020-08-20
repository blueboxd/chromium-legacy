// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

SecurePaymentConfirmationController::SecurePaymentConfirmationController()
    : view_(SecurePaymentConfirmationView::Create()) {}

SecurePaymentConfirmationController::~SecurePaymentConfirmationController() =
    default;

void SecurePaymentConfirmationController::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif  // OS_ANDROID
  DCHECK(view_);

  request_ = request;

  model_.set_verify_button_label(l10n_util::GetStringUTF16(
      IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
  model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));
  model_.set_progress_bar_visible(false);

  model_.set_title(l10n_util::GetStringUTF16(
      IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_PURCHASE));

  // TODO(crbug/1110322): Set the field values based on |request|.
  model_.set_merchant_label(
      l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_STORE_LABEL));

  model_.set_instrument_label(l10n_util::GetStringUTF16(
      IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));

  model_.set_total_label(
      l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TOTAL_LABEL));

  view_->ShowDialog(
      request->web_contents(), model_.GetWeakPtr(),
      base::BindOnce(&SecurePaymentConfirmationController::OnConfirm,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SecurePaymentConfirmationController::OnCancel,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SecurePaymentConfirmationController::CloseDialog() {
  if (!view_)
    return;

  view_->HideDialog();
}

void SecurePaymentConfirmationController::ShowProcessingSpinner() {
  if (!view_)
    return;

  model_.set_progress_bar_visible(true);
  view_->OnModelUpdated();
}

void SecurePaymentConfirmationController::OnDismiss() {}

void SecurePaymentConfirmationController::OnCancel() {
  if (!request_)
    return;

  request_->UserCancelled();
}

void SecurePaymentConfirmationController::OnConfirm() {
  if (!request_)
    return;

  request_->UserCancelled();
}

}  // namespace payments
