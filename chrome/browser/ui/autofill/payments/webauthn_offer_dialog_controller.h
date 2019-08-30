// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

// Per-tab controller lazily initialized when the WebauthnOfferDialogView is
// shown. Owned by the AuthenticatorRequestSheetView.
class WebauthnOfferDialogController : public AuthenticatorRequestSheetModel {
 public:
  explicit WebauthnOfferDialogController(
      AutofillClient::WebauthnOfferDialogCallback callback);
  ~WebauthnOfferDialogController() override;

  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsBackButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  base::Optional<base::string16> GetAdditionalDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;
  void OnBack() override;
  void OnAccept() override;
  void OnCancel() override;

 private:
  bool fetching_authentication_challenge_ = false;

  // Callback invoked when any button in the dialog is clicked. Note this repeating callback can be
  // run twice, since after the accept button is clicked, the dialog stays and the cancel button is
  // still clickable.
  AutofillClient::WebauthnOfferDialogCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_CONTROLLER_H_
