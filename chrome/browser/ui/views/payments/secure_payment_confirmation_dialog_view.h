// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/content/secure_payment_confirmation_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

// Draws the user interface in the secure payment confirmation flow. Owned by
// the SecurePaymentConfirmationController.
class SecurePaymentConfirmationDialogView
    : public SecurePaymentConfirmationView,
      public views::DialogDelegateView {
 public:
  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;
    virtual void OnDialogClosed() = 0;
    virtual void OnConfirmButtonPressed() = 0;
    virtual void OnCancelButtonPressed() = 0;
  };

  explicit SecurePaymentConfirmationDialogView(
      ObserverForTest* observer_for_test);
  ~SecurePaymentConfirmationDialogView() override;

  // SecurePaymentConfirmationView:
  void ShowDialog(content::WebContents* web_contents,
                  base::WeakPtr<SecurePaymentConfirmationModel> model,
                  VerifyCallback verify_callback,
                  CancelCallback cancel_callback) override;
  void OnModelUpdated() override;
  void HideDialog() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;

  // views::DialogDelegate:
  bool ShouldShowCloseButton() const override;

  base::WeakPtr<SecurePaymentConfirmationDialogView> GetWeakPtr();

 private:
  void OnDialogAccepted();
  void OnDialogCancelled();
  void OnDialogClosed();

  // May be null.
  ObserverForTest* observer_for_test_;

  VerifyCallback verify_callback_;
  CancelCallback cancel_callback_;

  base::WeakPtrFactory<SecurePaymentConfirmationDialogView> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_DIALOG_VIEW_H_
