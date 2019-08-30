// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

WebauthnOfferDialogController::WebauthnOfferDialogController(
    AutofillClient::WebauthnOfferDialogCallback callback)
    : callback_(callback) {}

WebauthnOfferDialogController::~WebauthnOfferDialogController() {
  callback_.Reset();
}

bool WebauthnOfferDialogController::IsActivityIndicatorVisible() const {
  return fetching_authentication_challenge_;
}

bool WebauthnOfferDialogController::IsBackButtonVisible() const {
  return false;
}

bool WebauthnOfferDialogController::IsCancelButtonVisible() const {
  return true;
}

base::string16 WebauthnOfferDialogController::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL);
}

bool WebauthnOfferDialogController::IsAcceptButtonVisible() const {
  return true;
}

bool WebauthnOfferDialogController::IsAcceptButtonEnabled() const {
  return !fetching_authentication_challenge_;
}

base::string16 WebauthnOfferDialogController::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_OK_BUTTON_LABEL);
}

const gfx::VectorIcon& WebauthnOfferDialogController::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark
             ? kWebauthnOfferDialogHeaderDarkIcon
             : kWebauthnOfferDialogHeaderIcon;
}

base::string16 WebauthnOfferDialogController::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE);
}

base::string16 WebauthnOfferDialogController::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION);
}

base::Optional<base::string16>
WebauthnOfferDialogController::GetAdditionalDescription() const {
  return base::nullopt;
}

ui::MenuModel* WebauthnOfferDialogController::GetOtherTransportsMenuModel() {
  return nullptr;
}

void WebauthnOfferDialogController::OnBack() {}

void WebauthnOfferDialogController::OnAccept() {
  DCHECK(callback_);
  callback_.Run(true);
}

void WebauthnOfferDialogController::OnCancel() {
  DCHECK(callback_);
  callback_.Run(false);
}

}  // namespace autofill
