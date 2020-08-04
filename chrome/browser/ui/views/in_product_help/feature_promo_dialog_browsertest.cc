// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"

class FeaturePromoDialogTest : public DialogBrowserTest {
 public:
  FeaturePromoDialogTest() = default;
  ~FeaturePromoDialogTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* app_menu_button = BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar_button_provider()
                                ->GetAppMenuButton();
    // We use an arbitrary string because there are no test-only
    // strings.
    int placeholder_string = IDS_REOPEN_TAB_PROMO;
    FeaturePromoBubbleParams bubble_params;
    bubble_params.body_string_specifier = placeholder_string;
    bubble_params.anchor_view = app_menu_button;
    bubble_params.arrow = views::BubbleBorder::TOP_RIGHT;
    FeaturePromoBubbleView::Create(std::move(bubble_params));
  }
};

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
