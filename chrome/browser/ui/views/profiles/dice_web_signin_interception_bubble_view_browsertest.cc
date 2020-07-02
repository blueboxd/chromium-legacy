// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "content/public/test/browser_test.h"

class DiceWebSigninInterceptionBubbleBrowserTest : public DialogBrowserTest {
 public:
  DiceWebSigninInterceptionBubbleBrowserTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    DCHECK(avatar_button);
    DiceWebSigninInterceptionBubbleView::CreateBubble(browser()->profile(),
                                                      avatar_button);
  }
};

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptionBubbleBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
