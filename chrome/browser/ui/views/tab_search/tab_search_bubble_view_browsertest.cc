// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/browser_test.h"

class TabSearchBubbleBrowserTest : public DialogBrowserTest {
 public:
  TabSearchBubbleBrowserTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    TabSearchBubbleView::CreateTabSearchBubble(browser()->profile(),
                                               browser_view->toolbar());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabSearchBubbleBrowserTest);
};

// Invokes a tab search bubble.
IN_PROC_BROWSER_TEST_F(TabSearchBubbleBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
