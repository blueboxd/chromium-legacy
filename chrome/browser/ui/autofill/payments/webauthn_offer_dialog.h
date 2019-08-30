// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace content {
class WebContents;
}

namespace autofill {

// Creates and shows the dialog to offer the option of using device's platform
// authenticator instead of CVC to verify the card in the future.
void ShowWebauthnOfferDialogView(
    content::WebContents* web_contents,
    AutofillClient::WebauthnOfferDialogCallback callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_WEBAUTHN_OFFER_DIALOG_H_
