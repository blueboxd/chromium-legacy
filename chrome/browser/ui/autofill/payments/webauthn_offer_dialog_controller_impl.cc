// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_view.h"

namespace autofill {

WebauthnOfferDialogControllerImpl::WebauthnOfferDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebauthnOfferDialogControllerImpl::~WebauthnOfferDialogControllerImpl() =
    default;

void WebauthnOfferDialogControllerImpl::ShowOfferDialog(
    AutofillClient::WebauthnOfferDialogCallback callback) {
  DCHECK(!dialog_view_);

  offer_dialog_callback_ = std::move(callback);
  dialog_view_ = WebauthnOfferDialogView::CreateAndShow(this);
}

bool WebauthnOfferDialogControllerImpl::CloseDialog() {
  if (!dialog_view_)
    return false;

  dialog_view_->Hide();
  return true;
}

void WebauthnOfferDialogControllerImpl::OnOkButtonClicked() {
  DCHECK(offer_dialog_callback_);
  offer_dialog_callback_.Run(/*did_accept=*/true);
  dialog_view_->RefreshContent();
}

void WebauthnOfferDialogControllerImpl::OnCancelButtonClicked() {
  DCHECK(offer_dialog_callback_);
  offer_dialog_callback_.Run(/*did_accept=*/false);
}

void WebauthnOfferDialogControllerImpl::OnDialogClosed() {
  dialog_view_ = nullptr;
  offer_dialog_callback_.Reset();
}

content::WebContents* WebauthnOfferDialogControllerImpl::GetWebContents() {
  return web_contents();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebauthnOfferDialogControllerImpl)

}  // namespace autofill
