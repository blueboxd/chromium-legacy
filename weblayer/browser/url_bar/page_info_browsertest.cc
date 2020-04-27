// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/android/page_info_client.h"
#include "components/page_info/page_info_delegate.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/url_bar/page_info_delegate_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class PageInfoBrowserTest : public WebLayerBrowserTest {
 protected:
  content::WebContents* GetWebContents() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents();
  }
};

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, PageInfoClientSet) {
  EXPECT_TRUE(page_info::GetPageInfoClient());
}

IN_PROC_BROWSER_TEST_F(PageInfoBrowserTest, ContentNotDisplayedInVrHeadset) {
  std::unique_ptr<PageInfoDelegate> page_info_delegate =
      page_info::GetPageInfoClient()->CreatePageInfoDelegate(GetWebContents());
  ASSERT_TRUE(page_info_delegate);
  EXPECT_FALSE(page_info_delegate->IsContentDisplayedInVrHeadset());
}

}  // namespace weblayer
