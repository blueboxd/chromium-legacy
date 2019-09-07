// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

WebauthnOfferDialogModel::WebauthnOfferDialogModel() {
  state_ = kOffer;
}

WebauthnOfferDialogModel::~WebauthnOfferDialogModel() = default;

bool WebauthnOfferDialogModel::IsActivityIndicatorVisible() const {
  return state_ == kPending;
}

bool WebauthnOfferDialogModel::IsBackButtonVisible() const {
  return false;
}

bool WebauthnOfferDialogModel::IsCancelButtonVisible() const {
  return true;
}

base::string16 WebauthnOfferDialogModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL);
}

bool WebauthnOfferDialogModel::IsAcceptButtonVisible() const {
  return true;
}

bool WebauthnOfferDialogModel::IsAcceptButtonEnabled() const {
  return state_ != kPending;
}

base::string16 WebauthnOfferDialogModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_OK_BUTTON_LABEL);
}

const gfx::VectorIcon& WebauthnOfferDialogModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark
             ? kWebauthnOfferDialogHeaderDarkIcon
             : kWebauthnOfferDialogHeaderIcon;
}

base::string16 WebauthnOfferDialogModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE);
}

base::string16 WebauthnOfferDialogModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION);
}

base::Optional<base::string16>
WebauthnOfferDialogModel::GetAdditionalDescription() const {
  return base::nullopt;
}

ui::MenuModel* WebauthnOfferDialogModel::GetOtherTransportsMenuModel() {
  return nullptr;
}

void WebauthnOfferDialogModel::OnBack() {}

void WebauthnOfferDialogModel::OnAccept() {
  state_ = kPending;
}

void WebauthnOfferDialogModel::OnCancel() {}

}  // namespace autofill
