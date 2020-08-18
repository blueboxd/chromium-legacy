// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CONTROLLER_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "components/payments/content/secure_payment_confirmation_view.h"

namespace payments {

class PaymentRequest;

// Controls the user interface in the secure payment confirmation flow.
class SecurePaymentConfirmationController {
 public:
  SecurePaymentConfirmationController();
  ~SecurePaymentConfirmationController();

  SecurePaymentConfirmationController(
      const SecurePaymentConfirmationController& other) = delete;
  SecurePaymentConfirmationController& operator=(
      const SecurePaymentConfirmationController& other) = delete;

  // Shows the dialog for the |request|.
  void ShowDialog(base::WeakPtr<PaymentRequest> request);

  // Closes the dialog.
  void CloseDialog();

  // Shows a "processing" spinner or progress bar in the dialog.
  void ShowProcessingSpinner();

  // Callbacks for user interaction.
  void OnDismiss();
  void OnCancel();
  void OnConfirm();

 private:
  base::WeakPtr<PaymentRequest> request_;

  SecurePaymentConfirmationModel model_;

  // On desktop, the SecurePaymentConfirmationView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  base::WeakPtr<SecurePaymentConfirmationView> view_;

  base::WeakPtrFactory<SecurePaymentConfirmationController> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CONTROLLER_H_
