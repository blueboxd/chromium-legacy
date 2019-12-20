// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace chrome {

using BrowserCommandsTest = InProcessBrowserTest;

// Verify that calling BookmarkCurrentTabIgnoringExtensionOverrides() just
// after closing all tabs doesn't cause a crash. https://crbug.com/799668
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, BookmarkCurrentTabAfterCloseTabs) {
  browser()->tab_strip_model()->CloseAllTabs();
  BookmarkCurrentTabIgnoringExtensionOverrides(browser());
}

class ReloadObserver : public content::WebContentsObserver {
 public:
  ~ReloadObserver() override = default;

  int load_count() const { return load_count_; }
  void SetWebContents(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  // content::WebContentsObserver
  void DidStartLoading() override { load_count_++; }

 private:
  int load_count_ = 0;
};

// Verify that all of selected tabs are refreshed after executing a reload
// command. https://crbug.com/862102
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, ReloadSelectedTabs) {
  constexpr char kUrl[] = "chrome://version/";
  constexpr int kTabCount = 3;
  std::vector<ReloadObserver> watcher_vec(kTabCount);
  for (int i = 0; i < kTabCount; i++) {
    AddTabAtIndexToBrowser(browser(), i + 1, GURL(kUrl),
                           ui::PAGE_TRANSITION_LINK, false);
    content::WebContents* tab =
        browser()->tab_strip_model()->GetWebContentsAt(i + 1);
    watcher_vec[i].SetWebContents(tab);
  }

  for (ReloadObserver& watcher : watcher_vec)
    EXPECT_EQ(0, watcher.load_count());

  // Add two tabs to the selection (the last one created remains selected) and
  // trigger a reload command on all of them.
  for (int i = 0; i < kTabCount - 1; i++)
    browser()->tab_strip_model()->ToggleSelectionAt(i + 1);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));

  int load_sum = 0;
  for (ReloadObserver& watcher : watcher_vec)
    load_sum += watcher.load_count();
  EXPECT_EQ(kTabCount, load_sum);
}

// Tests IDC_MOVE_TAB_TO_NEW_WINDOW. This is a browser test and not a unit test
// since it needs to create a new browser window, which doesn't work with a
// TestingProfile.
IN_PROC_BROWSER_TEST_F(BrowserCommandsTest, MoveTabToNewWindow) {
  GURL url1("chrome://version");
  GURL url2("chrome://about");
  ui_test_utils::NavigateToURL(browser(), url1);

  // Should be disabled with 1 tab.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));
  AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_LINK);
  // Two tabs is enough for it to be meaningful to pop one out.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW));

  BrowserList* browser_list = BrowserList::GetInstance();
  // Pre-command, assert that we have one browser, with two tabs, with the
  // url2 tab active.
  EXPECT_EQ(browser_list->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);

  chrome::ExecuteCommand(browser(), IDC_MOVE_TAB_TO_NEW_WINDOW);

  // Now we should have: two browsers, each with one tab (url1 in browser(),
  // and url2 in the new one).
  Browser* active_browser = browser_list->GetLastActive();
  EXPECT_EQ(browser_list->size(), 2u);
  EXPECT_NE(active_browser, browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(active_browser->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url1);
  EXPECT_EQ(active_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
            url2);
}

}  // namespace chrome
