// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/controls/webview/webview.h"

class WebUITabStripInteractiveTest : public InProcessBrowserTest {
 public:
  WebUITabStripInteractiveTest() {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~WebUITabStripInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
};

// Regression test for crbug.com/1027375.
IN_PROC_BROWSER_TEST_F(WebUITabStripInteractiveTest,
                       CanTypeInOmniboxAfterTabStripClose) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  WebUITabStripContainerView* const container = browser_view->webui_tab_strip();
  ASSERT_NE(nullptr, container);

  ui_test_utils::FocusView(browser(), VIEW_ID_OMNIBOX);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  OmniboxViewViews* const omnibox =
      browser_view->toolbar()->location_bar()->omnibox_view();
  omnibox->SetUserText(base::ASCIIToUTF16(""));

  container->SetVisibleForTesting(true);
  browser_view->Layout();

  // Make sure the tab strip's contents are fully loaded.
  views::WebView* const container_web_view = container->web_view_for_testing();
  ASSERT_TRUE(WaitForLoadStop(container_web_view->GetWebContents()));

  // Click in tab strip, then close it.
  base::RunLoop click_loop;
  ui_test_utils::MoveMouseToCenterAndPress(
      container_web_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, click_loop.QuitClosure());
  click_loop.Run();
  container->SetVisibleForTesting(false);

  // The omnibox should still be focused and should accept keyboard input.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  EXPECT_EQ(base::ASCIIToUTF16("a"), omnibox->GetText());
}
