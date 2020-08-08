// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/browser/prerender_manager.h"
#include "weblayer/browser/no_state_prefetch/prerender_link_manager_factory.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace weblayer {

class NoStatePrefetchBrowserTest : public WebLayerBrowserTest {
 protected:
  content::BrowserContext* GetBrowserContext() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents()->GetBrowserContext();
  }
};

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, CreatePrerenderManager) {
  auto* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(GetBrowserContext());
  EXPECT_TRUE(prerender_manager);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, CreatePrerenderLinkManager) {
  auto* prerender_link_manager =
      PrerenderLinkManagerFactory::GetForBrowserContext(GetBrowserContext());
  EXPECT_TRUE(prerender_link_manager);
}

}  // namespace weblayer