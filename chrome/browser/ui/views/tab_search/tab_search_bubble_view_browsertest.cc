// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"

class TabSearchBubbleBrowserTest : public DialogBrowserTest {
 public:
  TabSearchBubbleBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kTabSearch);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    AppendTab(chrome::kChromeUISettingsURL);
    AppendTab(chrome::kChromeUIHistoryURL);
    AppendTab(chrome::kChromeUIBookmarksURL);
    TabSearchBubbleView::CreateTabSearchBubble(browser());
  }

  void AppendTab(std::string url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(TabSearchBubbleBrowserTest);
};

// Invokes a tab search bubble.
IN_PROC_BROWSER_TEST_F(TabSearchBubbleBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
