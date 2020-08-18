// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/secure_payment_confirmation_model.h"

namespace payments {

SecurePaymentConfirmationDialogView::SecurePaymentConfirmationDialogView(
    ObserverForTest* observer_for_test)
    : observer_for_test_(observer_for_test) {}
SecurePaymentConfirmationDialogView::~SecurePaymentConfirmationDialogView() =
    default;

void SecurePaymentConfirmationDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<SecurePaymentConfirmationModel> model,
    VerifyCallback verify_callback,
    CancelCallback cancel_callback) {
  DCHECK(model);
  model_ = model;

  OnModelUpdated();

  // Set the dialog size. This is just for demonstration and will no longer be
  // necessary once the layout is complete.
  const int dialog_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const gfx::Size dialog_size(dialog_width, 300);
  SetPreferredSize(dialog_size);

  verify_callback_ = std::move(verify_callback);
  cancel_callback_ = std::move(cancel_callback);

  SetAcceptCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();
}

void SecurePaymentConfirmationDialogView::OnDialogAccepted() {
  std::move(verify_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnConfirmButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogClosed() {
  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnModelUpdated() {
  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->verify_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, model_->verify_button_enabled());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_enabled());
}

void SecurePaymentConfirmationDialogView::HideDialog() {
  GetWidget()->Close();
}

ui::ModalType SecurePaymentConfirmationDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool SecurePaymentConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

base::WeakPtr<SecurePaymentConfirmationDialogView>
SecurePaymentConfirmationDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
