// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/webauthn_offer_dialog_view_impl.h"

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

WebauthnOfferDialogViewImpl::WebauthnOfferDialogViewImpl(
    WebauthnOfferDialogController* controller)
    : controller_(controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  std::unique_ptr<WebauthnOfferDialogModel> model =
      std::make_unique<WebauthnOfferDialogModel>();
  model_ = model.get();
  sheet_view_ =
      AddChildView(CreateSheetViewForAutofillWebAuthn(std::move(model)));
  sheet_view_->ReInitChildViews();
}

WebauthnOfferDialogViewImpl::~WebauthnOfferDialogViewImpl() = default;

// static
WebauthnOfferDialogView* WebauthnOfferDialogView::CreateAndShow(
    WebauthnOfferDialogController* controller) {
  WebauthnOfferDialogViewImpl* dialog =
      new WebauthnOfferDialogViewImpl(controller);
  constrained_window::ShowWebModalDialogViews(dialog,
                                              controller->GetWebContents());
  return dialog;
}

void WebauthnOfferDialogViewImpl::Hide() {
  GetWidget()->Close();
}

// TODO(crbug.com/991037): Need to resize the dialog after reinitializing the
// dialog to error dialog since the string content in the error dialog is
// different.
void WebauthnOfferDialogViewImpl::RefreshContent() {
  sheet_view_->ReInitChildViews();
  sheet_view_->InvalidateLayout();
  Layout();
  DialogModelChanged();
}

gfx::Size WebauthnOfferDialogViewImpl::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool WebauthnOfferDialogViewImpl::Accept() {
  model_->OnAccept();
  controller_->OnOkButtonClicked();
  return false;
}

bool WebauthnOfferDialogViewImpl::Cancel() {
  model_->OnCancel();
  controller_->OnCancelButtonClicked();
  return true;
}

bool WebauthnOfferDialogViewImpl::Close() {
  return true;
}

// TODO(crbug.com/991037): Fetching authentication challenge will send a request
// to Payments server, and it can fail sometimes. For the error case, the dialog
// should be updated and the OK button should not be shown.
int WebauthnOfferDialogViewImpl::GetDialogButtons() const {
  DCHECK(model_->IsAcceptButtonVisible() && model_->IsCancelButtonVisible());
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 WebauthnOfferDialogViewImpl::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? model_->GetAcceptButtonLabel()
                                        : model_->GetCancelButtonLabel();
}

bool WebauthnOfferDialogViewImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? model_->IsAcceptButtonEnabled()
                                        : true;
}

ui::ModalType WebauthnOfferDialogViewImpl::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 WebauthnOfferDialogViewImpl::GetWindowTitle() const {
  return model_->GetStepTitle();
}

bool WebauthnOfferDialogViewImpl::ShouldShowWindowTitle() const {
  return false;
}

bool WebauthnOfferDialogViewImpl::ShouldShowCloseButton() const {
  return false;
}

void WebauthnOfferDialogViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

}  // namespace autofill
