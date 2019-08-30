// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/webauthn_offer_dialog_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

WebauthnOfferDialogView::WebauthnOfferDialogView(
    content::WebContents* web_contents,
    AutofillClient::WebauthnOfferDialogCallback callback) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  std::unique_ptr<WebauthnOfferDialogController> controller =
      std::make_unique<WebauthnOfferDialogController>(callback);
  controller_ = controller.get();
  sheet_view_ =
      AddChildView(CreateSheetViewForAutofillWebAuthn(std::move(controller)));
  sheet_view_->ReInitChildViews();
}

WebauthnOfferDialogView::~WebauthnOfferDialogView() = default;

// static
void ShowWebauthnOfferDialogView(
    content::WebContents* web_contents,
    AutofillClient::WebauthnOfferDialogCallback callback) {
  WebauthnOfferDialogView* dialog =
      new WebauthnOfferDialogView(web_contents, callback);
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
}

gfx::Size WebauthnOfferDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool WebauthnOfferDialogView::Accept() {
  controller_->OnAccept();
  // TODO(crbug.com/991037): Make the dialog stay and show the progression
  // indicator.
  return true;
}

bool WebauthnOfferDialogView::Cancel() {
  controller_->OnCancel();
  return true;
}

bool WebauthnOfferDialogView::Close() {
  return true;
}

// TODO(crbug.com/991037): Fetching authentication challenge will send a request
// to Payments server, and it can fail sometimes. For the error case, the dialog
// should be updated and the OK button should not be shown.
int WebauthnOfferDialogView::GetDialogButtons() const {
  DCHECK(controller_->IsAcceptButtonVisible() &&
         controller_->IsCancelButtonVisible());
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 WebauthnOfferDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? controller_->GetAcceptButtonLabel()
                                        : controller_->GetCancelButtonLabel();
}

bool WebauthnOfferDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? controller_->IsAcceptButtonEnabled()
                                        : true;
}

ui::ModalType WebauthnOfferDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 WebauthnOfferDialogView::GetWindowTitle() const {
  return controller_->GetStepTitle();
}

bool WebauthnOfferDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool WebauthnOfferDialogView::ShouldShowCloseButton() const {
  return false;
}

}  // namespace autofill
