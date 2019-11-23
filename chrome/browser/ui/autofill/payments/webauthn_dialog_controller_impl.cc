// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_view.h"

namespace autofill {

WebauthnDialogControllerImpl::WebauthnDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebauthnDialogControllerImpl::~WebauthnDialogControllerImpl() {
  // This part of code is executed only if browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // WebauthnDialogViewImpl::dtor() being called, but the reference to
  // controller is not reset. Need to reset via WebauthnDialogViewImpl::Hide()
  // to avoid crash.
  if (dialog_model_)
    dialog_model_->SetDialogState(WebauthnDialogModel::DialogState::kInactive);
}

void WebauthnDialogControllerImpl::ShowOfferDialog(
    AutofillClient::WebauthnOfferDialogCallback offer_dialog_callback) {
  DCHECK(!dialog_model_);

  offer_dialog_callback_ = std::move(offer_dialog_callback);
  dialog_view_ = WebauthnDialogView::CreateAndShow(this);
  dialog_model_ = dialog_view_->GetDialogModel();
}

bool WebauthnDialogControllerImpl::CloseDialog() {
  if (!dialog_model_)
    return false;

  dialog_model_->SetDialogState(WebauthnDialogModel::DialogState::kInactive);
  return true;
}

void WebauthnDialogControllerImpl::UpdateDialogWithError() {
  dialog_model_->SetDialogState(WebauthnDialogModel::DialogState::kError);
  offer_dialog_callback_.Reset();
}

void WebauthnDialogControllerImpl::OnDialogClosed() {
  dialog_model_ = nullptr;
  dialog_view_ = nullptr;
  offer_dialog_callback_.Reset();
}

content::WebContents* WebauthnDialogControllerImpl::GetWebContents() {
  return web_contents();
}

void WebauthnDialogControllerImpl::OnOkButtonClicked() {
  DCHECK(offer_dialog_callback_);
  offer_dialog_callback_.Run(/*did_accept=*/true);
  dialog_model_->SetDialogState(WebauthnDialogModel::DialogState::kPending);
}

void WebauthnDialogControllerImpl::OnCancelButtonClicked() {
  DCHECK(offer_dialog_callback_);
  offer_dialog_callback_.Run(/*did_accept=*/false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebauthnDialogControllerImpl)

}  // namespace autofill
