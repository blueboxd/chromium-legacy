// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class WebauthnDialogModel;
class WebauthnDialogView;

// Implementation of the per-tab controller to control the
// WebauthnDialogView. Lazily initialized when used.
class WebauthnDialogControllerImpl
    : public WebauthnDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<WebauthnDialogControllerImpl> {
 public:
  ~WebauthnDialogControllerImpl() override;

  void ShowOfferDialog(
      AutofillClient::WebauthnOfferDialogCallback offer_dialog_callback);
  bool CloseDialog();
  void UpdateDialogWithError();

  // WebauthnDialogController:
  void OnOkButtonClicked() override;
  void OnCancelButtonClicked() override;
  void OnDialogClosed() override;
  content::WebContents* GetWebContents() override;

  WebauthnDialogView* dialog_view() { return dialog_view_; }

 protected:
  explicit WebauthnDialogControllerImpl(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<WebauthnDialogControllerImpl>;

  // Callback invoked when any button in the offer dialog is clicked. Note this
  // repeating callback can be run twice, since after the accept button is
  // clicked, the offer dialog stays and the cancel button is still clickable.
  AutofillClient::WebauthnOfferDialogCallback offer_dialog_callback_;

  WebauthnDialogModel* dialog_model_ = nullptr;
  WebauthnDialogView* dialog_view_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(WebauthnDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_DIALOG_CONTROLLER_IMPL_H_
