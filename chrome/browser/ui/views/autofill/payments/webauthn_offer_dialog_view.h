// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "ui/views/window/dialog_delegate.h"

class AuthenticatorRequestSheetView;

namespace content {
class WebContents;
}

namespace autofill {

// The view of the dialog that offers the option to use device's platform
// authenticator. It is shown automatically after card unmasked details are
// obtained and filled into the form.
class WebauthnOfferDialogView : public views::DialogDelegateView {
 public:
  WebauthnOfferDialogView(content::WebContents* web_contents,
                          AutofillClient::WebauthnOfferDialogCallback callback);
  ~WebauthnOfferDialogView() override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

 private:
  friend void ShowWebauthnOfferDialogView(
      content::WebContents* web_contents,
      AutofillClient::WebauthnOfferDialogCallback callback);

  // Reference to the controller. The controller is owned by the
  // AuthenticatorRequestSheetView. Since this dialog view owns the sheet view,
  // the controller is destroyed only when the dialog is.
  WebauthnOfferDialogController* controller_ = nullptr;
  AuthenticatorRequestSheetView* sheet_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_VIEW_H_
