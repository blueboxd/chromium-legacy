// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/chromeos/kiosk_next/kiosk_next_browser_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_types.h"
#include "url/gurl.h"

namespace {
const GURL kUrl1("https://www.url1.com");
const GURL kUrl2("https://www.url2.com");
const GURL kUrl3("https://www.url3.com");
}  // namespace

class KioskNextBrowserFactoryTest : public InProcessBrowserTest {
 public:
  KioskNextBrowserFactoryTest() {}
  ~KioskNextBrowserFactoryTest() override = default;

  KioskNextBrowserList& GetBrowserList() {
    return browser_factory_.browser_list_;
  }

 protected:
  KioskNextBrowserFactory browser_factory_{2 /* max_browsers_allowed */};

  DISALLOW_COPY_AND_ASSIGN(KioskNextBrowserFactoryTest);
};

IN_PROC_BROWSER_TEST_F(KioskNextBrowserFactoryTest, TestBrowserParams) {
  // Adds a new KioskNextBrowser instance.
  Browser* browser = browser_factory_.CreateForWebsite(kUrl1);
  EXPECT_NE(browser, nullptr);
  EXPECT_EQ(browser->initial_show_state(), ui::SHOW_STATE_MAXIMIZED);
  EXPECT_FALSE(browser->is_type_tabbed());
  EXPECT_TRUE(GetBrowserList().IsKioskNextBrowser(browser));
  EXPECT_EQ(GetBrowserList().GetBrowserForWebsite(kUrl1), browser);
}

IN_PROC_BROWSER_TEST_F(KioskNextBrowserFactoryTest, TestFactoryCreateMethods) {
  ASSERT_TRUE(GetBrowserList().CanAddBrowser());
  EXPECT_EQ(GetBrowserList().GetBrowserForWebsite(kUrl1), nullptr);

  // Adds a new KioskNextBrowser instance.
  browser_factory_.CreateForWebsite(kUrl1);
  EXPECT_TRUE(GetBrowserList().CanAddBrowser());
  Browser* browser_2 = browser_factory_.CreateForWebsite(kUrl2);
  EXPECT_NE(browser_2, nullptr);
  EXPECT_EQ(GetBrowserList().GetBrowserEntry(browser_2)->browser, browser_2);

  EXPECT_FALSE(GetBrowserList().CanAddBrowser());
  Browser* browser_3 = browser_factory_.CreateForWebsite(kUrl3);
  EXPECT_EQ(GetBrowserList().GetBrowserForWebsite(kUrl3), browser_3);

  // The LRU browser has been removed.
  EXPECT_EQ(GetBrowserList().GetBrowserForWebsite(kUrl1), nullptr);

  // Make browser_3 is the lru browser.
  GetBrowserList().OnBrowserSetLastActive(browser_2);

  browser_factory_.CreateForWebsite(kUrl1);

  // LRU browser has been removed.
  EXPECT_EQ(GetBrowserList().GetBrowserForWebsite(kUrl3), nullptr);
}
