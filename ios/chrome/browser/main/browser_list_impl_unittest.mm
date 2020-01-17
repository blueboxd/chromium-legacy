// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_list_impl.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/browser_list_observer.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/main/test_browser_list_observer.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class BrowserListImplTest : public PlatformTest {
 protected:
  BrowserListImplTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_list_ =
        BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BrowserList* browser_list_;
};

// Test main add/remove logic.
TEST_F(BrowserListImplTest, AddRemoveBrowsers) {
  // Browser list should start empty
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());

  TestBrowser browser_1(chrome_browser_state_.get());

  // Adding a browser should result in it appearing in the list.
  browser_list_->AddBrowser(&browser_1);
  std::set<Browser*> browsers = browser_list_->AllRegularBrowsers();
  EXPECT_EQ(1UL, browsers.size());
  auto found_browser = browsers.find(&browser_1);
  EXPECT_EQ(&browser_1, *found_browser);

  TestBrowser browser_2(chrome_browser_state_.get());

  // Removing a browser not in the list is a no-op.
  browser_list_->RemoveBrowser(&browser_2);
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // More than one browset can be added to the list.
  browser_list_->AddBrowser(&browser_2);
  EXPECT_EQ(2UL, browser_list_->AllRegularBrowsers().size());

  // Removing a browser works -- the list gets smaller, and the removed browser
  // isn't on it.
  browser_list_->RemoveBrowser(&browser_2);
  browsers = browser_list_->AllRegularBrowsers();
  EXPECT_EQ(1UL, browsers.size());
  found_browser = browsers.find(&browser_2);
  EXPECT_EQ(browsers.end(), found_browser);

  // Removing a browser a second time does nothing.
  browser_list_->RemoveBrowser(&browser_2);
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // Removing the last browser, even multiple times, works as expected.
  browser_list_->RemoveBrowser(&browser_1);
  browser_list_->RemoveBrowser(&browser_1);
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
}

// Test regular/incognito interactions
TEST_F(BrowserListImplTest, AddRemoveIncognitoBrowsers) {
  // Incognito browser list starts empty.
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());

  TestBrowser browser_1(chrome_browser_state_.get());

  ios::ChromeBrowserState* incognito_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  TestBrowser incognito_browser_1(incognito_browser_state);

  // Adding a regular browser doesn't affect the incognito list.
  browser_list_->AddBrowser(&browser_1);
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());

  // Adding an incognito browser doesn't affect the regular list.
  browser_list_->AddIncognitoBrowser(&incognito_browser_1);
  EXPECT_EQ(1UL, browser_list_->AllIncognitoBrowsers().size());
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // An added incognito browser is in the list.
  std::set<Browser*> browsers = browser_list_->AllIncognitoBrowsers();
  auto found_browser = browsers.find(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, *found_browser);

  // Removing browsers from the wrong list has no effect.
  browser_list_->RemoveBrowser(&incognito_browser_1);
  browser_list_->RemoveIncognitoBrowser(&browser_1);
  EXPECT_EQ(1UL, browser_list_->AllIncognitoBrowsers().size());
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // Removing browsers from the correct lists works as expected.
  browser_list_->RemoveBrowser(&browser_1);
  browser_list_->RemoveIncognitoBrowser(&incognito_browser_1);
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
}

// Test that values returned from AllRegularBrowsers and AllIncognitoBrowsers
// aren't affected by subsequent changes to the browser list.
TEST_F(BrowserListImplTest, AllBrowserValuesDontChange) {
  Browser* browser_1 = new TestBrowser(chrome_browser_state_.get());

  // Add a browser and get the current set of browsers.
  browser_list_->AddBrowser(browser_1);
  std::set<Browser*> browsers = browser_list_->AllRegularBrowsers();
  EXPECT_EQ(1UL, browsers.size());

  // Remove the browser.
  browser_list_->RemoveBrowser(browser_1);
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());

  // Check that the browser is still in the set.
  auto found_browser = browsers.find(browser_1);
  EXPECT_EQ(1UL, browsers.size());
  EXPECT_EQ(browser_1, *found_browser);
}

// Check that an observer is informed of additions and removals to both the
// regular and incognito browser lists.
TEST_F(BrowserListImplTest, BrowserListObserver) {
  TestBrowserListObserver* observer = new TestBrowserListObserver;
  browser_list_->AddObserver(observer);

  Browser* browser_1 = new TestBrowser(chrome_browser_state_.get());
  ios::ChromeBrowserState* incognito_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  Browser* incognito_browser_1 = new TestBrowser(incognito_browser_state);

  // Check that a regular addition is observerd.
  browser_list_->AddBrowser(browser_1);
  EXPECT_EQ(browser_1, observer->GetLastAddedBrowser());
  EXPECT_EQ(1UL, observer->GetLastBrowsers().size());

  // Check that a no-op  removal is *not* observerd.
  browser_list_->RemoveBrowser(incognito_browser_1);
  EXPECT_EQ(nullptr, observer->GetLastRemovedBrowser());

  // Check that a regular removal is observerd.
  browser_list_->RemoveBrowser(browser_1);
  EXPECT_EQ(browser_1, observer->GetLastRemovedBrowser());
  EXPECT_EQ(0UL, observer->GetLastBrowsers().size());

  // Check that an incognito addition is observerd.
  browser_list_->AddIncognitoBrowser(incognito_browser_1);
  EXPECT_EQ(incognito_browser_1, observer->GetLastAddedIncognitoBrowser());
  EXPECT_EQ(1UL, observer->GetLastIncognitoBrowsers().size());

  // Check that an incognito removal is observerd.
  browser_list_->RemoveIncognitoBrowser(incognito_browser_1);
  EXPECT_EQ(incognito_browser_1, observer->GetLastRemovedIncognitoBrowser());
  EXPECT_EQ(0UL, observer->GetLastIncognitoBrowsers().size());

  browser_list_->RemoveObserver(observer);
}
