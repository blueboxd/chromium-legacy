// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"

namespace autofill {

class WebauthnOfferDialogBrowsertest : public DialogBrowserTest {
 public:
  WebauthnOfferDialogBrowsertest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    ShowWebauthnOfferDialogView(web_contents, base::DoNothing());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebauthnOfferDialogBrowsertest);
};

IN_PROC_BROWSER_TEST_F(WebauthnOfferDialogBrowsertest, InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace autofill
