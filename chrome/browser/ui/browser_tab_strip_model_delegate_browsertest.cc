// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace chrome {

using BrowserTabStripModelDelegateTest = InProcessBrowserTest;

// Tests the "Move Tab to New Window" tab context menu command.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateTest, MoveTabsToNewWindow) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://version");
  GURL url2("chrome://about");
  ui_test_utils::NavigateToURL(browser(), url1);

  // Moving a tab from a single tab window to a new tab window is a no-op.
  // TODO(lgrey): When moving to existing windows is implemented, add a case
  // for this test that asserts we *can* move to an existing window from a
  // single tab window.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0}));

  AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK);

  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({1}));
  // Moving *all* the tabs in a window to a new window is a no-op.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0, 1}));

  BrowserList* browser_list = BrowserList::GetInstance();

  // Precondition: there's currently one browser with two tabs.
  EXPECT_EQ(browser_list->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  // Execute this on a background tab to ensure that the code path can handle
  // other tabs besides the active one.
  delegate->MoveTabsToNewWindow({0});

  // Now there are two browsers, each with one tab and the new browser is
  // active.
  Browser* active_browser = browser_list->GetLastActive();
  EXPECT_EQ(browser_list->size(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(active_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(active_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url1);
}

// Tests the "Move Tab to New Window" tab context menu command with multiple
// tabs selected.
IN_PROC_BROWSER_TEST_F(BrowserTabStripModelDelegateTest,
                       MoveMultipleTabsToNewWindow) {
  std::unique_ptr<TabStripModelDelegate> delegate =
      std::make_unique<BrowserTabStripModelDelegate>(browser());

  GURL url1("chrome://version");
  GURL url2("chrome://about");
  GURL url3("chrome://terms");
  ui_test_utils::NavigateToURL(browser(), url1);

  // Moving a tab from a single tab window to a new tab window is a no-op.
  // TODO(jugallag): When moving to existing windows is implemented, add a case
  // for this test that asserts we *can* move to an existing window from a
  // single tab window.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0}));

  AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK);
  AddTabAtIndex(2, url3, ui::PAGE_TRANSITION_LINK);

  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({1}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({2}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0, 1}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({0, 2}));
  EXPECT_TRUE(delegate->CanMoveTabsToWindow({1, 2}));
  // Moving *all* the tabs in a window to a new window is a no-op.
  EXPECT_FALSE(delegate->CanMoveTabsToWindow({0, 1, 2}));

  BrowserList* browser_list = BrowserList::GetInstance();

  // Precondition: there's currently one browser with three tabs.
  EXPECT_EQ(browser_list->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url3);

  // Execute this on a background tab to ensure that the code path can handle
  // other tabs besides the active one.
  delegate->MoveTabsToNewWindow({0, 2});

  // Now there are two browsers, with one or two tabs and the new browser is
  // active.
  Browser* active_browser = browser_list->GetLastActive();
  EXPECT_EQ(browser_list->size(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(active_browser->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);
  EXPECT_EQ(active_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url3);
}

}  // namespace chrome
